#include "widgets/circular_color_picker.h"
#include <Arduino.h>
#include <cmath>

lv_obj_t* CircularColorPicker::create(lv_obj_t* parent, lv_coord_t size, uint8_t brightness) {
    // Create container
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, size, size);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    // Allocate picker data
    PickerData* data = new PickerData();
    data->size = size;
    data->hue = 0;
    data->saturation = 100;
    data->brightness = brightness;
    data->dragging = false;

    // Create canvas for color circle
    data->canvas = lv_canvas_create(container);
    lv_obj_set_size(data->canvas, size, size);
    lv_obj_center(data->canvas);

    // Allocate buffer for canvas with alpha channel (ARGB8888 format)
    size_t buffer_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(size, size);
    uint8_t* cbuf = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    if (!cbuf) {
        Serial.println("❌ Failed to allocate canvas buffer");
        delete data;
        return nullptr;
    }
    lv_canvas_set_buffer(data->canvas, cbuf, size, size, LV_IMG_CF_TRUE_COLOR_ALPHA);

    // Draw the color circle
    draw_color_circle(data->canvas, size, brightness);

    // Create cursor with enhanced 3D effect
    data->cursor = lv_obj_create(container);
    lv_obj_remove_style_all(data->cursor);
    lv_obj_set_size(data->cursor, 14, 14);
    lv_obj_set_style_radius(data->cursor, LV_RADIUS_CIRCLE, 0);

    // Outer white border for contrast
    lv_obj_set_style_border_width(data->cursor, 3, 0);
    lv_obj_set_style_border_color(data->cursor, lv_color_white(), 0);
    lv_obj_set_style_border_opa(data->cursor, LV_OPA_90, 0);

    // Semi-transparent center
    lv_obj_set_style_bg_color(data->cursor, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(data->cursor, LV_OPA_30, 0);

    // Shadow for depth
    lv_obj_set_style_shadow_width(data->cursor, 8, 0);
    lv_obj_set_style_shadow_color(data->cursor, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(data->cursor, LV_OPA_50, 0);
    lv_obj_set_style_shadow_spread(data->cursor, 0, 0);
    lv_obj_set_style_shadow_ofs_x(data->cursor, 0, 0);
    lv_obj_set_style_shadow_ofs_y(data->cursor, 2, 0);

    // Store data in container
    lv_obj_set_user_data(container, data);

    // Add event handler
    lv_obj_add_event_cb(container, event_handler, LV_EVENT_ALL, nullptr);

    // Initialize cursor at edge (full saturation, hue 0 = red)
    update_cursor_position(container);

    return container;
}

void CircularColorPicker::draw_color_circle(lv_obj_t* canvas, lv_coord_t size, uint8_t brightness) {
    lv_coord_t center = size / 2;
    lv_coord_t radius = (size / 2) - 2;

    // Clear entire canvas first
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    for (lv_coord_t y = 0; y < size; y++) {
        for (lv_coord_t x = 0; x < size; x++) {
            int dx = x - center;
            int dy = y - center;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist <= radius) {
                // Calculate angle and saturation
                float angle = atan2f(dy, dx);
                uint16_t hue = (uint16_t)((angle + M_PI) * 180.0f / M_PI);
                uint8_t saturation = (uint8_t)((dist / radius) * 100.0f);

                // Set pixel with color
                lv_color_t color = lv_color_hsv_to_rgb(hue, saturation, brightness);
                lv_canvas_set_px_color(canvas, x, y, color);
                lv_canvas_set_px_opa(canvas, x, y, LV_OPA_COVER);
            }
        }
    }

    lv_obj_invalidate(canvas);
}

void CircularColorPicker::update_cursor_position(lv_obj_t* obj) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return;

    lv_coord_t center = data->size / 2;
    lv_coord_t radius = data->size / 2;

    // Convert saturation to radius distance
    float dist = (data->saturation / 100.0f) * radius;

    // Convert hue to angle (0-360° -> radians)
    float angle = (data->hue * M_PI / 180.0f) - M_PI;

    // Calculate cursor position (14x14 cursor, so -7 for centering)
    lv_coord_t cursor_x = center + (lv_coord_t)(dist * cosf(angle)) - 7;
    lv_coord_t cursor_y = center + (lv_coord_t)(dist * sinf(angle)) - 7;

    lv_obj_set_pos(data->cursor, cursor_x, cursor_y);
}

void CircularColorPicker::event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = lv_event_get_target(e);
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return;

    if (code == LV_EVENT_PRESSED) {
        data->dragging = true;

        // Block scrolling on all parent objects while dragging
        lv_obj_t* parent = lv_obj_get_parent(obj);
        while (parent) {
            lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
            parent = lv_obj_get_parent(parent);
        }

        lv_indev_t* indev = lv_indev_get_act();
        lv_point_t point;
        lv_indev_get_point(indev, &point);

        // Convert to local coordinates relative to the container
        lv_area_t coords;
        lv_obj_get_coords(obj, &coords);
        lv_coord_t x = point.x - coords.x1;
        lv_coord_t y = point.y - coords.y1;

        handle_touch(obj, x, y);

        // Trigger VALUE_CHANGED event
        lv_event_send(obj, LV_EVENT_VALUE_CHANGED, nullptr);
    } else if (code == LV_EVENT_PRESSING) {
        lv_indev_t* indev = lv_indev_get_act();
        lv_point_t point;
        lv_indev_get_point(indev, &point);

        // Convert to local coordinates relative to the container
        lv_area_t coords;
        lv_obj_get_coords(obj, &coords);
        lv_coord_t x = point.x - coords.x1;
        lv_coord_t y = point.y - coords.y1;

        handle_touch(obj, x, y);

        // Trigger VALUE_CHANGED event
        lv_event_send(obj, LV_EVENT_VALUE_CHANGED, nullptr);
    } else if (code == LV_EVENT_RELEASED) {
        data->dragging = false;

        // Re-enable scrolling on parent objects after dragging
        lv_obj_t* parent = lv_obj_get_parent(obj);
        while (parent) {
            // Only re-enable scrolling for the root container (should have LV_DIR_VER)
            if (lv_obj_has_flag(parent, LV_OBJ_FLAG_SCROLL_ONE)) {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
            }
            // Check if this was originally scrollable by checking scroll direction
            if (lv_obj_get_scroll_dir(parent) != LV_DIR_NONE) {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
            }
            parent = lv_obj_get_parent(parent);
        }
    } else if (code == LV_EVENT_DELETE) {
        // Cleanup
        if (data->canvas) {
            lv_img_dsc_t* dsc = lv_canvas_get_img(data->canvas);
            if (dsc && dsc->data) {
                heap_caps_free((void*)dsc->data);
            }
        }
        delete data;
    }
}

void CircularColorPicker::handle_touch(lv_obj_t* obj, lv_coord_t x, lv_coord_t y) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return;

    lv_coord_t center = data->size / 2;
    lv_coord_t radius = data->size / 2;

    // Calculate distance from center
    int dx = x - center;
    int dy = y - center;
    float dist = sqrtf(dx * dx + dy * dy);

    // Clamp to circle
    if (dist > radius) {
        dist = radius;
    }

    // Calculate saturation (0 at center, 100 at edge)
    data->saturation = (uint8_t)((dist / radius) * 100.0f);

    // Calculate hue from angle
    float angle = atan2f(dy, dx);
    data->hue = (uint16_t)((angle + M_PI) * 180.0f / M_PI); // 0-360

    // Update cursor
    update_cursor_position(obj);
}

void CircularColorPicker::set_rgb(lv_obj_t* obj, lv_color_t color) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return;

    // Convert RGB to HSV
    lv_color_hsv_t hsv = lv_color_rgb_to_hsv(
        LV_COLOR_GET_R(color),
        LV_COLOR_GET_G(color),
        LV_COLOR_GET_B(color)
    );

    data->hue = hsv.h;
    data->saturation = hsv.s;
    // Don't update brightness from external color

    update_cursor_position(obj);
}

void CircularColorPicker::set_hsv(lv_obj_t* obj, lv_color_hsv_t hsv) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return;

    data->hue = hsv.h;
    data->saturation = hsv.s;
    // Don't update brightness from external HSV

    update_cursor_position(obj);
}

lv_color_t CircularColorPicker::get_rgb(lv_obj_t* obj) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return lv_color_black();

    return lv_color_hsv_to_rgb(data->hue, data->saturation, data->brightness);
}

lv_color_hsv_t CircularColorPicker::get_hsv(lv_obj_t* obj) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) {
        return {0, 0, 0};
    }

    lv_color_hsv_t hsv;
    hsv.h = data->hue;
    hsv.s = data->saturation;
    hsv.v = data->brightness;
    return hsv;
}

void CircularColorPicker::set_brightness(lv_obj_t* obj, uint8_t brightness) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return;

    data->brightness = brightness;

    // Redraw the color circle with new brightness
    draw_color_circle(data->canvas, data->size, brightness);
}

uint8_t CircularColorPicker::get_brightness(lv_obj_t* obj) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return 70;

    return data->brightness;
}

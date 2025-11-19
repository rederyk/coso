#include "widgets/circular_color_picker.h"
#include <Arduino.h>
#include <cmath>
#include "ui/ui_symbols.h"
#include "utils/logger.h"

namespace {
constexpr uint32_t kDoubleTapWindowMs = 350;
constexpr float kDayBrightnessScale = 1.0f;      // Giorno: luminosità massima
constexpr uint8_t kNightEdgeBrightness = 50;     // Notte: bordo fisso al 50%
constexpr float kNightCurveExponent = 1.8f;      // Esponente per curva (ridotto per transizione più graduale)
constexpr uint8_t kMinBrightness = 5;
}  // namespace

uint8_t CircularColorPicker::apply_mode_brightness(uint8_t brightness, bool night_mode) {
    if (night_mode) {
        // Modalità notte: valore non usato, gestiamo il gradiente direttamente in draw_color_circle
        return kNightEdgeBrightness;
    } else {
        // Modalità giorno: usa brightness al massimo
        int value = static_cast<int>(roundf(brightness * kDayBrightnessScale));
        if (value < kMinBrightness) value = kMinBrightness;
        if (value > 100) value = 100;
        return static_cast<uint8_t>(value);
    }
}

uint8_t CircularColorPicker::compute_mode_brightness(const PickerData* data) {
    if (!data) return kMinBrightness;
    return apply_mode_brightness(data->brightness, data->night_mode);
}

lv_obj_t* CircularColorPicker::create(lv_obj_t* parent, lv_coord_t size, uint8_t brightness, bool enable_mode_toggle) {
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
    data->night_mode = false;
    data->last_tap_tick = 0;
    data->mode_toggle_enabled = enable_mode_toggle;

    // Create canvas for color circle
    data->canvas = lv_canvas_create(container);
    lv_obj_set_size(data->canvas, size, size);
    lv_obj_center(data->canvas);

    // Allocate buffer for canvas with alpha channel (ARGB8888 format)
    size_t buffer_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(size, size);
    uint8_t* cbuf = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    if (!cbuf) {
        Logger::getInstance().error(UI_SYMBOL_ERROR " Failed to allocate canvas buffer");
        delete data;
        return nullptr;
    }
    lv_canvas_set_buffer(data->canvas, cbuf, size, size, LV_IMG_CF_TRUE_COLOR_ALPHA);

    // Draw the color circle
    draw_color_circle(data->canvas, size, brightness, data->night_mode);

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

void CircularColorPicker::draw_color_circle(lv_obj_t* canvas,
                                            lv_coord_t size,
                                            uint8_t brightness,
                                            bool night_mode) {
    lv_coord_t center = size / 2;
    lv_coord_t radius = (size / 2) - 2;
    uint8_t effective_brightness = apply_mode_brightness(brightness, night_mode);

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
                float normalized = dist / radius;
                if (normalized < 0.0f) normalized = 0.0f;
                if (normalized > 1.0f) normalized = 1.0f;

                // Saturazione: 0 al centro, 100 al bordo (sempre uguale per entrambe le modalità)
                uint8_t saturation = (uint8_t)(normalized * 100.0f);
                if (saturation > 100) saturation = 100;

                uint8_t pixel_value;

                if (night_mode) {
                    // Modalità notte: curva esponenziale per colori più scuri e nero vero al centro
                    // normalized^2.5 crea una curva che resta scura più a lungo
                    // Al centro (normalized=0): 0^2.5 = 0 (nero puro)
                    // Al bordo (normalized=1): 1^2.5 = 1 → 50%
                    float brightness_factor = powf(normalized, kNightCurveExponent);
                    pixel_value = static_cast<uint8_t>(kNightEdgeBrightness * brightness_factor);
                } else {
                    // Modalità giorno: cerchio HSV normale con brightness massima
                    pixel_value = effective_brightness;
                }

                if (pixel_value > 100) pixel_value = 100;

                // Set pixel with color
                lv_color_t color = lv_color_hsv_to_rgb(hue, saturation, pixel_value);
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
    } else if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED) {
        if (data->mode_toggle_enabled) {
            uint32_t now = lv_tick_get();
            if (data->last_tap_tick != 0 && lv_tick_elaps(data->last_tap_tick) < kDoubleTapWindowMs) {
                data->last_tap_tick = 0;
                toggle_display_mode(obj);
            } else {
                data->last_tap_tick = now;
            }
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

void CircularColorPicker::toggle_display_mode(lv_obj_t* obj) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data || !data->canvas) return;

    data->night_mode = !data->night_mode;
    draw_color_circle(data->canvas, data->size, data->brightness, data->night_mode);
    lv_event_send(obj, LV_EVENT_VALUE_CHANGED, nullptr);
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

    uint8_t value;
    if (data->night_mode) {
        // In modalità notte, calcola il brightness basato sulla saturazione (= distanza radiale)
        // Saturazione 0% (centro) → brightness 0 (nero)
        // Saturazione 100% (bordo) → brightness 50%
        float normalized = data->saturation / 100.0f;
        float brightness_factor = powf(normalized, kNightCurveExponent);
        value = static_cast<uint8_t>(kNightEdgeBrightness * brightness_factor);
    } else {
        // Modalità giorno: brightness uniforme
        value = compute_mode_brightness(data);
    }

    return lv_color_hsv_to_rgb(data->hue, data->saturation, value);
}

lv_color_hsv_t CircularColorPicker::get_hsv(lv_obj_t* obj) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) {
        return {0, 0, 0};
    }

    lv_color_hsv_t hsv;
    hsv.h = data->hue;
    hsv.s = data->saturation;

    if (data->night_mode) {
        // In modalità notte, calcola il brightness basato sulla saturazione (= distanza radiale)
        float normalized = data->saturation / 100.0f;
        float brightness_factor = powf(normalized, kNightCurveExponent);
        hsv.v = static_cast<uint8_t>(kNightEdgeBrightness * brightness_factor);
    } else {
        // Modalità giorno: brightness uniforme
        hsv.v = compute_mode_brightness(data);
    }

    return hsv;
}

void CircularColorPicker::set_brightness(lv_obj_t* obj, uint8_t brightness) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return;

    data->brightness = brightness;

    // Redraw the color circle with new brightness
    draw_color_circle(data->canvas, data->size, brightness, data->night_mode);
}

uint8_t CircularColorPicker::get_brightness(lv_obj_t* obj) {
    PickerData* data = (PickerData*)lv_obj_get_user_data(obj);
    if (!data) return 70;

    return data->brightness;
}

#pragma once

#include <lvgl.h>

/**
 * @brief Custom 2D circular color picker widget
 *
 * Features:
 * - Center: low saturation (gray/black)
 * - Edge: high saturation (vivid colors)
 * - Angle: hue (0-360°)
 * - Radius: saturation (0-100%)
 * - Fixed brightness value (can be adjusted externally)
 */
class CircularColorPicker {
public:
    /**
     * @brief Create a circular color picker
     * @param parent Parent LVGL object
     * @param size Diameter of the picker in pixels
     * @param brightness Initial brightness value (0-100)
     * @return The created LVGL canvas object
     */
    static lv_obj_t* create(lv_obj_t* parent, lv_coord_t size, uint8_t brightness = 70);

    /**
     * @brief Set the color of the picker (updates cursor position)
     * @param obj The picker object
     * @param color RGB color to set
     */
    static void set_rgb(lv_obj_t* obj, lv_color_t color);

    /**
     * @brief Set the color using HSV values
     * @param obj The picker object
     * @param hsv HSV color to set
     */
    static void set_hsv(lv_obj_t* obj, lv_color_hsv_t hsv);

    /**
     * @brief Get the current RGB color
     * @param obj The picker object
     * @return Current RGB color
     */
    static lv_color_t get_rgb(lv_obj_t* obj);

    /**
     * @brief Get the current HSV color
     * @param obj The picker object
     * @return Current HSV color
     */
    static lv_color_hsv_t get_hsv(lv_obj_t* obj);

    /**
     * @brief Set the brightness value (redraws the picker)
     * @param obj The picker object
     * @param brightness Brightness value (0-100)
     */
    static void set_brightness(lv_obj_t* obj, uint8_t brightness);

    /**
     * @brief Get the current brightness value
     * @param obj The picker object
     * @return Current brightness value (0-100)
     */
    static uint8_t get_brightness(lv_obj_t* obj);

private:
    struct PickerData {
        lv_obj_t* canvas;           // Canvas for drawing the color circle
        lv_obj_t* cursor;           // Cursor indicator
        lv_coord_t size;            // Diameter of the picker
        uint16_t hue;               // Current hue (0-359)
        uint8_t saturation;         // Current saturation (0-100)
        uint8_t brightness;         // Base brightness (0-100)
        bool dragging;              // Touch state
        bool night_mode;            // Whether dark mode is active
        uint32_t last_tap_tick;     // Last tap timestamp for double-tap detection
    };

    static void draw_color_circle(lv_obj_t* canvas, lv_coord_t size, uint8_t brightness, bool night_mode);
    static void update_cursor_position(lv_obj_t* obj);
    static void event_handler(lv_event_t* e);
    static void handle_touch(lv_obj_t* obj, lv_coord_t x, lv_coord_t y);
    static void toggle_display_mode(lv_obj_t* obj);
    static uint8_t compute_mode_brightness(const PickerData* data);
    static uint8_t apply_mode_brightness(uint8_t brightness, bool night_mode);
};

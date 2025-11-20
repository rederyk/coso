#pragma once

#include <lvgl.h>
#include <functional>

/**
 * @brief Global LVGL Keyboard Manager
 *
 * Provides a singleton keyboard instance accessible from any text input field
 * across the firmware. Manages keyboard visibility, text area association,
 * and submission callbacks.
 *
 * Usage:
 * @code
 * // Initialize once in main.cpp after LVGL init
 * KeyboardManager::getInstance().init(lv_scr_act());
 *
 * // Show keyboard for a text area
 * lv_obj_add_event_cb(textarea, [](lv_event_t* e) {
 *     auto* ta = lv_event_get_target(e);
 *     KeyboardManager::getInstance().showForTextArea(ta);
 * }, LV_EVENT_FOCUSED, nullptr);
 * @endcode
 */
class KeyboardManager {
public:
    /**
     * @brief Get singleton instance
     */
    static KeyboardManager& getInstance();

    /**
     * @brief Initialize keyboard with parent container
     * @param parent Parent object (typically lv_scr_act())
     */
    void init(lv_obj_t* parent);

    /**
     * @brief Show keyboard for a specific text area
     * @param textarea Target text area to associate with keyboard
     * @param onSubmit Optional callback when user presses OK/Enter
     */
    void showForTextArea(lv_obj_t* textarea, std::function<void(const char*)> onSubmit = nullptr);

    /**
     * @brief Hide keyboard
     */
    void hide();

    /**
     * @brief Check if keyboard is currently visible
     * @return true if keyboard is visible
     */
    bool isVisible() const;

    /**
     * @brief Set keyboard mode
     * @param mode Keyboard mode (text, number, special chars)
     */
    void setMode(lv_keyboard_mode_t mode);

private:
    KeyboardManager() = default;
    ~KeyboardManager() = default;
    KeyboardManager(const KeyboardManager&) = delete;
    KeyboardManager& operator=(const KeyboardManager&) = delete;

    /**
     * @brief Keyboard event handler
     */
    static void handleKeyboardEvent(lv_event_t* e);

    lv_obj_t* keyboard_ = nullptr;
    lv_obj_t* current_textarea_ = nullptr;
    std::function<void(const char*)> submit_callback_ = nullptr;
};

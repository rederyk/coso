#include "keyboard_manager.h"
#include "utils/logger.h"

KeyboardManager& KeyboardManager::getInstance() {
    static KeyboardManager instance;
    return instance;
}

void KeyboardManager::init(lv_obj_t* parent) {
    if (!parent) {
        Logger::getInstance().error("[KeyboardManager] init: parent is null");
        return;
    }

    // Create persistent keyboard instance
    keyboard_ = lv_keyboard_create(parent);
    if (!keyboard_) {
        Logger::getInstance().error("[KeyboardManager] Failed to create keyboard");
        return;
    }

    // Set keyboard size - full width, limited height for small displays
    // Max height of 50% of screen for 240x320 displays
    lv_obj_set_size(keyboard_, lv_pct(100), lv_pct(50));

    // Remove any scrollable flags from keyboard
    lv_obj_clear_flag(keyboard_, LV_OBJ_FLAG_SCROLLABLE);

    // Position keyboard at bottom of screen
    lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Reduce padding to make keyboard more compact
    lv_obj_set_style_pad_all(keyboard_, 2, 0);
    lv_obj_set_style_pad_gap(keyboard_, 2, 0);

    // Set high z-index to ensure keyboard appears above other content
    lv_obj_move_foreground(keyboard_);

    // Add event callback for keyboard events
    lv_obj_add_event_cb(keyboard_, handleKeyboardEvent, LV_EVENT_ALL, this);

    // Hide keyboard initially
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);

    Logger::getInstance().info("[KeyboardManager] Initialized successfully");
}

void KeyboardManager::showForTextArea(lv_obj_t* textarea, std::function<void(const char*)> onSubmit) {
    if (!keyboard_) {
        Logger::getInstance().error("[KeyboardManager] showForTextArea: keyboard not initialized");
        return;
    }

    if (!textarea) {
        Logger::getInstance().error("[KeyboardManager] showForTextArea: textarea is null");
        return;
    }

    // Store current textarea and callback
    current_textarea_ = textarea;
    submit_callback_ = onSubmit;

    // Associate keyboard with text area
    lv_keyboard_set_textarea(keyboard_, textarea);

    // Show keyboard
    lv_obj_clear_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);

    // Ensure keyboard is positioned correctly at bottom
    lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Bring keyboard to foreground
    lv_obj_move_foreground(keyboard_);

    // Force an update to ensure proper positioning
    lv_obj_update_layout(keyboard_);

    Logger::getInstance().debug("[KeyboardManager] Keyboard shown for text area");
}

void KeyboardManager::hide() {
    if (!keyboard_) {
        return;
    }

    // Unassociate from text area
    lv_keyboard_set_textarea(keyboard_, nullptr);

    // Hide keyboard
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);

    // Clear state
    current_textarea_ = nullptr;
    submit_callback_ = nullptr;

    Logger::getInstance().debug("[KeyboardManager] Keyboard hidden");
}

bool KeyboardManager::isVisible() const {
    if (!keyboard_) {
        return false;
    }
    return !lv_obj_has_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
}

void KeyboardManager::setMode(lv_keyboard_mode_t mode) {
    if (!keyboard_) {
        Logger::getInstance().error("[KeyboardManager] setMode: keyboard not initialized");
        return;
    }

    lv_keyboard_set_mode(keyboard_, mode);
}

void KeyboardManager::handleKeyboardEvent(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    auto* manager = static_cast<KeyboardManager*>(lv_event_get_user_data(e));

    if (!manager) {
        return;
    }

    // Handle LV_EVENT_READY (OK/Enter pressed)
    if (code == LV_EVENT_READY) {
        Logger::getInstance().debug("[KeyboardManager] User pressed OK/Enter");

        // Call submit callback if provided
        if (manager->submit_callback_ && manager->current_textarea_) {
            const char* text = lv_textarea_get_text(manager->current_textarea_);
            manager->submit_callback_(text);
        }

        // Hide keyboard
        manager->hide();
    }
    // Handle LV_EVENT_CANCEL (Cancel/Close pressed)
    else if (code == LV_EVENT_CANCEL) {
        Logger::getInstance().debug("[KeyboardManager] User pressed Cancel");

        // Just hide keyboard without calling callback
        manager->hide();
    }
}

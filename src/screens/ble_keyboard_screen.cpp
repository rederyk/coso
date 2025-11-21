#include "screens/ble_keyboard_screen.h"
#include "core/app_manager.h"
#include "screens/ble_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "layouts/us_keyboard_layout.h"

void BleKeyboardScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);

    // Header with a back button
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 4, 0);

    lv_obj_t* back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 60, 40);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Live Keyboard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_left(title, 10, 0);

    // Create the keyboard
    keyboard = lv_keyboard_create(root);
    lv_obj_set_size(keyboard, lv_pct(100), lv_pct(90));
    lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void BleKeyboardScreen::onShow() {
    // Nothing specific to do on show
}

void BleKeyboardScreen::onHide() {
    // Nothing specific to do on hide
}

void BleKeyboardScreen::back_button_event_cb(lv_event_t* e) {
    AppManager::getInstance()->launchApp("ble_remote");
}

void BleKeyboardScreen::keyboard_event_cb(lv_event_t* e) {
    auto* screen = static_cast<BleKeyboardScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    lv_obj_t* kb = lv_event_get_target(e);
    uint16_t btn_id = lv_keyboard_get_selected_btn(kb);
    if (btn_id == LV_BTNMATRIX_BTN_NONE) return;

    BleManager& bleManager = BleManager::getInstance();
    lv_keyboard_mode_t mode = lv_keyboard_get_mode(kb);

    // Handle special keys first
    const char* txt = lv_keyboard_get_btn_text(kb, btn_id);
    if (txt) {
        if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
            bleManager.sendKey(KEY_BACKSPACE);
            return;
        }
        if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0 || strcmp(txt, "Enter") == 0) {
            bleManager.sendKey(KEY_ENTER);
            return;
        }
        // Ignore mode-switching buttons by checking their text
        if (strcmp(txt, "abc") == 0 || strcmp(txt, "ABC") == 0 || strcmp(txt, "1#") == 0 ||
            strcmp(txt, LV_SYMBOL_OK) == 0 || strcmp(txt, LV_SYMBOL_CLOSE) == 0) {
            return;
        }
    }


    // Handle mode-specific mappings
    // In SPECIAL mode, button IDs are different
    KeyInfo key_info = {0, 0};

    if (mode == LV_KEYBOARD_MODE_SPECIAL && txt && strlen(txt) == 1) {
        // Special mode: map common special characters directly
        char ch = txt[0];
        switch (ch) {
            case '1': key_info = {KEY_1, 0}; break;
            case '2': key_info = {KEY_2, 0}; break;
            case '3': key_info = {KEY_3, 0}; break;
            case '4': key_info = {KEY_4, 0}; break;
            case '5': key_info = {KEY_5, 0}; break;
            case '6': key_info = {KEY_6, 0}; break;
            case '7': key_info = {KEY_7, 0}; break;
            case '8': key_info = {KEY_8, 0}; break;
            case '9': key_info = {KEY_9, 0}; break;
            case '0': key_info = {KEY_0, 0}; break;
            case '+': key_info = {KEY_EQUAL, KEY_MOD_LSHIFT}; break;
            case '=': key_info = {KEY_EQUAL, 0}; break;
            case '/': key_info = {KEY_SLASH, 0}; break;
            case '*': key_info = {KEY_8, KEY_MOD_LSHIFT}; break;
            case '%': key_info = {KEY_5, KEY_MOD_LSHIFT}; break;
            case '!': key_info = {KEY_1, KEY_MOD_LSHIFT}; break;
            case '?': key_info = {KEY_SLASH, KEY_MOD_LSHIFT}; break;
            case '#': key_info = {KEY_3, KEY_MOD_LSHIFT}; break;
            case '<': key_info = {KEY_COMMA, KEY_MOD_LSHIFT}; break;
            case '>': key_info = {KEY_DOT, KEY_MOD_LSHIFT}; break;
            case '&': key_info = {KEY_7, KEY_MOD_LSHIFT}; break;
            case '@': key_info = {KEY_2, KEY_MOD_LSHIFT}; break;
            case '$': key_info = {KEY_4, KEY_MOD_LSHIFT}; break;
            case '(': key_info = {KEY_9, KEY_MOD_LSHIFT}; break;
            case ')': key_info = {KEY_0, KEY_MOD_LSHIFT}; break;
            case '{': key_info = {KEY_LEFTBRACE, KEY_MOD_LSHIFT}; break;
            case '}': key_info = {KEY_RIGHTBRACE, KEY_MOD_LSHIFT}; break;
            case '[': key_info = {KEY_LEFTBRACE, 0}; break;
            case ']': key_info = {KEY_RIGHTBRACE, 0}; break;
            case ';': key_info = {KEY_SEMICOLON, 0}; break;
            case '"': key_info = {KEY_APOSTROPHE, KEY_MOD_LSHIFT}; break;
            case '\'': key_info = {KEY_APOSTROPHE, 0}; break;
            case '\\': key_info = {KEY_BACKSLASH, 0}; break;
            case ' ': key_info = {KEY_SPACE, 0}; break;
        }
    } else {
        // Text mode: use the layout map
        key_info = USKeyboardLayout.getKey(btn_id);
    }

    if (key_info.keycode != 0) {
        uint8_t modifier = key_info.modifier;

        // Only add SHIFT for letter keys in uppercase mode
        // Don't add SHIFT if the key already has a modifier (like _ which is SHIFT+MINUS)
        if (mode == LV_KEYBOARD_MODE_TEXT_UPPER && key_info.modifier == 0) {
            // Check if it's a letter key (A-Z)
            if (key_info.keycode >= KEY_A && key_info.keycode <= KEY_Z) {
                modifier |= KEY_MOD_LSHIFT;
            }
        }

        Logger::getInstance().debugf("[BleKeyboardScreen] Mapped btn_id %d ('%s') mode=%d -> keycode 0x%02X, mod 0x%02X",
                                     btn_id, txt ? txt : "?", mode, key_info.keycode, modifier);
        bleManager.sendKey(key_info.keycode, modifier);
    } else {
        // Fallback for keys not in the map but with valid text
        if (txt && strlen(txt) > 0) {
             Logger::getInstance().warnf("[BleKeyboardScreen] No keymap for btn_id %d, text='%s', mode=%d",
                                        btn_id, txt, mode);
            //bleManager.sendText(txt);
        }
    }
}

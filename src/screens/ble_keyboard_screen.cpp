#include "screens/ble_keyboard_screen.h"
#include "core/app_manager.h"
#include "screens/ble_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"

// Standard HID keycodes
constexpr uint8_t KEY_ENTER = 0x28;
constexpr uint8_t KEY_BACKSPACE = 0x2a;
constexpr uint8_t KEY_TAB = 0x2b;

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

    const char* txt = lv_keyboard_get_btn_text(kb, btn_id);
    if (txt == nullptr) return;

    BleManager& bleManager = BleManager::getInstance();

    // Handle special keys
    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        bleManager.sendKey(KEY_BACKSPACE);
        return;
    }
    if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0 || strcmp(txt, "Enter") == 0) {
        bleManager.sendKey(KEY_ENTER);
        return;
    }
    if (strcmp(txt, "Tab") == 0) {
        bleManager.sendKey(KEY_TAB);
        return;
    }

    // Ignore mode-switching buttons
    if (strcmp(txt, "abc") == 0 || strcmp(txt, "ABC") == 0 || strcmp(txt, "1#") == 0 || strcmp(txt, LV_SYMBOL_OK) == 0 || strcmp(txt, LV_SYMBOL_CLOSE) == 0) {
        return;
    }

    // Send the character as text
    if (strlen(txt) > 0) {
        bleManager.sendText(txt);
    }
}

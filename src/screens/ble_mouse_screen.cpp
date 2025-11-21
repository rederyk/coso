#include "screens/ble_mouse_screen.h"
#include "core/app_manager.h"
#include "screens/ble_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include <cstdio>

namespace {
constexpr uint8_t MOUSE_BTN_LEFT = 1 << 0;
constexpr uint8_t MOUSE_BTN_RIGHT = 1 << 1;
constexpr uint8_t MOUSE_BTN_MIDDLE = 1 << 2;

lv_obj_t* create_control_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 40);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_pad_all(btn, 8, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);

    return btn;
}

} // namespace

void BleMouseScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_pad_row(root, 10, 0);

    // Header
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* back_btn = create_control_button(header, LV_SYMBOL_LEFT " Back", back_button_event_cb, this);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Full-screen Mouse");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_left(title, 10, 0);

    // Touchpad area
    touchpad_area = lv_obj_create(root);
    lv_obj_remove_style_all(touchpad_area);
    lv_obj_set_width(touchpad_area, lv_pct(100));
    lv_obj_set_flex_grow(touchpad_area, 1);
    lv_obj_set_style_radius(touchpad_area, 12, 0);
    lv_obj_set_style_bg_color(touchpad_area, lv_color_hex(0x2c2c2c), 0);
    lv_obj_set_style_bg_opa(touchpad_area, LV_OPA_COVER, 0);
    lv_obj_add_flag(touchpad_area, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(touchpad_area, touchpad_event_cb, LV_EVENT_ALL, this);

    lv_obj_t* pad_hint = lv_label_create(touchpad_area);
    lv_label_set_text(pad_hint, "Drag to move cursor");
    lv_obj_set_style_text_color(pad_hint, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(pad_hint, LV_ALIGN_CENTER, 0, 0);
    
    hint_label = lv_label_create(touchpad_area);
    lv_label_set_text(hint_label, "");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(hint_label, LV_ALIGN_TOP_MID, 0, 10);

    // Button controls
    lv_obj_t* button_row = lv_obj_create(root);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_width(button_row, lv_pct(100));
    lv_obj_set_height(button_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(button_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(button_row, 0, 0);

    lv_obj_t* left_btn = create_control_button(button_row, "Left Click", click_button_cb, this);
    lv_obj_set_user_data(left_btn, (void*)(uintptr_t)MOUSE_BTN_LEFT);

    lv_obj_t* middle_btn = create_control_button(button_row, "Middle", click_button_cb, this);
    lv_obj_set_user_data(middle_btn, (void*)(uintptr_t)MOUSE_BTN_MIDDLE);

    lv_obj_t* right_btn = create_control_button(button_row, "Right Click", click_button_cb, this);
    lv_obj_set_user_data(right_btn, (void*)(uintptr_t)MOUSE_BTN_RIGHT);

    lv_obj_t* scroll_up_btn = create_control_button(button_row, LV_SYMBOL_UP " Scroll", wheel_button_cb, this);
    lv_obj_set_user_data(scroll_up_btn, (void*)(intptr_t)1);
    
    lv_obj_t* scroll_down_btn = create_control_button(button_row, LV_SYMBOL_DOWN " Scroll", wheel_button_cb, this);
    lv_obj_set_user_data(scroll_down_btn, (void*)(intptr_t)-1);
}

void BleMouseScreen::back_button_event_cb(lv_event_t* e) {
    AppManager::getInstance()->launchApp("ble_remote");
}

void BleMouseScreen::touchpad_event_cb(lv_event_t* e) {
    auto* screen = static_cast<BleMouseScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSING) {
        return;
    }

    lv_indev_t* indev = lv_event_get_indev(e);
    if (!indev) return;

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    screen->dispatchMouse(static_cast<int8_t>(vect.x), static_cast<int8_t>(vect.y));
}

void BleMouseScreen::click_button_cb(lv_event_t* e) {
    auto* screen = static_cast<BleMouseScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    uint8_t buttons = static_cast<uint8_t>((uintptr_t)lv_obj_get_user_data(lv_event_get_target(e)));
    screen->dispatchClick(buttons);
}

void BleMouseScreen::wheel_button_cb(lv_event_t* e) {
    auto* screen = static_cast<BleMouseScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    int dir = static_cast<int>((intptr_t)lv_obj_get_user_data(lv_event_get_target(e)));
    screen->dispatchMouse(0, 0, dir > 0 ? 10 : -10, 0);
}

void BleMouseScreen::dispatchMouse(int8_t dx, int8_t dy, int8_t wheel, uint8_t buttons) {
    if (hint_label) {
        char buf[64];
        snprintf(buf, sizeof(buf), "dx:%d, dy:%d, wheel:%d", dx, dy, wheel);
        lv_label_set_text(hint_label, buf);
    }
    BleManager::getInstance().sendMouseMove(dx, dy, wheel, buttons);
}

void BleMouseScreen::dispatchClick(uint8_t buttons) {
    BleManager::getInstance().mouseClick(buttons);
}

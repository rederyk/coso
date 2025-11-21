#pragma once

#include "core/screen.h"
#include <lvgl.h>

class BleMouseScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;

private:
    static void back_button_event_cb(lv_event_t* e);
    static void touchpad_event_cb(lv_event_t* e);
    static void click_button_cb(lv_event_t* e);
    static void wheel_button_cb(lv_event_t* e);
    
    void dispatchMouse(int8_t dx, int8_t dy, int8_t wheel = 0, uint8_t buttons = 0);
    void dispatchClick(uint8_t buttons);

    lv_obj_t* touchpad_area = nullptr;
    lv_obj_t* hint_label = nullptr;
};

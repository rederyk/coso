#pragma once

#include "core/screen.h"
#include <lvgl.h>

class BleKeyboardScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    static void keyboard_event_cb(lv_event_t* e);
    static void back_button_event_cb(lv_event_t* e);

    lv_obj_t* keyboard = nullptr;
};

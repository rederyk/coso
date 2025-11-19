#pragma once

#include "core/screen.h"
#include <lvgl.h>

class SdExplorerScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void refreshCardInfo();
    void populateFileList();
    void showMessage(const char* text);
    void hideMessage();
    void performFormat();

    static void onRefreshClicked(lv_event_t* event);
    static void onFormatClicked(lv_event_t* event);
    static void onFormatConfirm(lv_event_t* event);
    static void onTimerTick(lv_timer_t* timer);

    lv_obj_t* status_label = nullptr;
    lv_obj_t* capacity_label = nullptr;
    lv_obj_t* type_label = nullptr;
    lv_obj_t* file_list = nullptr;
    lv_obj_t* message_label = nullptr;
    lv_timer_t* refresh_timer = nullptr;

    lv_obj_t* pending_msgbox = nullptr;

    static constexpr uint32_t REFRESH_MS = 5000;
};

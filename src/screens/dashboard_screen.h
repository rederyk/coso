#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"
#include "widgets/clock_widget.h"
#include "widgets/system_info_widget.h"

class DashboardScreen : public Screen {
public:
    ~DashboardScreen() override;
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void applyTheme(const SettingsSnapshot& snapshot);
    void updateStatusIcons();
    static void statusUpdateTimer(lv_timer_t* timer);

    SystemInfoWidget system_widget;
    ClockWidget clock_widget;
    lv_obj_t* header_label = nullptr;
    lv_obj_t* status_bar = nullptr;
    lv_obj_t* wifi_status_label = nullptr;
    lv_obj_t* ble_status_label = nullptr;
    lv_obj_t* sd_status_label = nullptr;
    lv_obj_t* layout = nullptr;
    lv_timer_t* status_timer = nullptr;

    uint32_t settings_listener_id = 0;
};

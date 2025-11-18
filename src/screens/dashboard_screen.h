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

    SystemInfoWidget system_widget;
    ClockWidget clock_widget;
    lv_obj_t* header_label = nullptr;
    lv_obj_t* layout = nullptr;

    uint32_t settings_listener_id = 0;
};

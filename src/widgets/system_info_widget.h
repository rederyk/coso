#pragma once

#include "widgets/dashboard_widget.h"

class SystemInfoWidget : public DashboardWidget {
public:
    SystemInfoWidget();
    ~SystemInfoWidget() override;

    void create(lv_obj_t* parent) override;
    void update() override;

private:
    lv_obj_t* heap_label = nullptr;
    lv_obj_t* uptime_label = nullptr;
    lv_timer_t* refresh_timer = nullptr;

    static void timerCallback(lv_timer_t* timer);
};

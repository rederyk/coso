#pragma once

#include "widgets/dashboard_widget.h"

class ClockWidget : public DashboardWidget {
public:
    ClockWidget();
    ~ClockWidget() override;

    void create(lv_obj_t* parent) override;
    void update() override;

private:
    lv_obj_t* label = nullptr;
    lv_timer_t* refresh_timer = nullptr;
    static void timerCallback(lv_timer_t* timer);
};

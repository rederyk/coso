#pragma once

#include "core/screen.h"
#include "widgets/clock_widget.h"
#include "widgets/system_info_widget.h"

class DashboardScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;

private:
    SystemInfoWidget system_widget;
    ClockWidget clock_widget;
};

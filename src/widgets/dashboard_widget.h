#pragma once

#include <lvgl.h>
#include "utils/lvgl_mutex.h"

class DashboardWidget {
public:
    virtual ~DashboardWidget();

    virtual void create(lv_obj_t* parent) = 0;
    virtual void update() = 0;

protected:
    lv_obj_t* container = nullptr;
};

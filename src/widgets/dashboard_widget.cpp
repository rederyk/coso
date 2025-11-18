#include "widgets/dashboard_widget.h"

DashboardWidget::~DashboardWidget() {
    if (container) {
        lv_obj_del(container);
        container = nullptr;
    }
}

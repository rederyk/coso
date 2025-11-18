#pragma once

#include "core/screen.h"

class SettingsScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    lv_obj_t* brightness_label = nullptr;
    lv_obj_t* version_label = nullptr;
};

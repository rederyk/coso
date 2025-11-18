#pragma once

#include "core/screen.h"

class InfoScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    lv_obj_t* chip_label = nullptr;
    lv_obj_t* freq_label = nullptr;
    lv_obj_t* flash_label = nullptr;
};

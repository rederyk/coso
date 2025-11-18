#pragma once

#include <lvgl.h>

class Screen {
public:
    virtual ~Screen();

    virtual void build(lv_obj_t* parent) = 0;
    virtual void onShow() {}
    virtual void onHide() {}
    void destroyRoot();

    lv_obj_t* getRoot() const { return root; }

protected:
    lv_obj_t* root = nullptr;
};

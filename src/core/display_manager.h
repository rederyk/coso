#pragma once

#include <lvgl.h>

class TFT_eSPI;

class DisplayManager {
public:
    static DisplayManager& getInstance();

    void begin(TFT_eSPI* tft,
               lv_disp_draw_buf_t* draw_buf,
               void (*flush_cb)(lv_disp_drv_t*, const lv_area_t*, lv_color_t*));
    void applyOrientation(bool landscape, bool force = false);
    lv_obj_t* getOverlayLayer();
    lv_obj_t* getLauncherLayer();

    lv_coord_t getWidth() const;
    lv_coord_t getHeight() const;
    bool isLandscape() const { return landscape_; }
    lv_disp_t* getDisplay() const { return disp_; }

private:
    DisplayManager() = default;
    void logOrientation(lv_coord_t width, lv_coord_t height) const;
    void refreshOverlayLayer(lv_coord_t width, lv_coord_t height);
    void refreshLauncherLayer(lv_coord_t width, lv_coord_t height);

    TFT_eSPI* tft_ = nullptr;
    lv_disp_draw_buf_t* draw_buf_ = nullptr;
    lv_disp_drv_t disp_drv_;
    lv_disp_t* disp_ = nullptr;
    lv_obj_t* overlay_layer_ = nullptr;
    lv_obj_t* launcher_layer_ = nullptr;
    bool landscape_ = true;
    bool initialized_ = false;
};

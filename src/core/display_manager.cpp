#include "core/display_manager.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

DisplayManager& DisplayManager::getInstance() {
    static DisplayManager instance;
    return instance;
}

void DisplayManager::begin(TFT_eSPI* tft,
                           lv_disp_draw_buf_t* draw_buf,
                           void (*flush_cb)(lv_disp_drv_t*, const lv_area_t*, lv_color_t*)) {
    if (initialized_) {
        return;
    }

    tft_ = tft;
    draw_buf_ = draw_buf;

    lv_disp_drv_init(&disp_drv_);
    disp_drv_.draw_buf = draw_buf_;
    disp_drv_.flush_cb = flush_cb;
    disp_drv_.hor_res = LV_HOR_RES_MAX;
    disp_drv_.ver_res = LV_VER_RES_MAX;

    disp_ = lv_disp_drv_register(&disp_drv_);
    initialized_ = true;

    Serial.println("[Display] LVGL display driver initialized");
    getOverlayLayer();
}

void DisplayManager::applyOrientation(bool landscape, bool force) {
    if (!initialized_) {
        return;
    }
    if (!force && landscape == landscape_) {
        return;
    }

    landscape_ = landscape;
    lv_coord_t width = landscape ? LV_HOR_RES_MAX : LV_VER_RES_MAX;
    lv_coord_t height = landscape ? LV_VER_RES_MAX : LV_HOR_RES_MAX;

    if (tft_) {
        uint8_t rotation = landscape ? 1 : 0;
        tft_->setRotation(rotation);
    }

    disp_drv_.hor_res = width;
    disp_drv_.ver_res = height;
    lv_disp_drv_update(disp_, &disp_drv_);

    if (lv_obj_t* screen = lv_disp_get_scr_act(disp_)) {
        lv_obj_set_size(screen, width, height);
    }

    refreshOverlayLayer(width, height);
    refreshLauncherLayer(width, height);
    logOrientation(width, height);
}

lv_coord_t DisplayManager::getWidth() const {
    if (initialized_) {
        return disp_drv_.hor_res;
    }
    return LV_HOR_RES_MAX;
}

lv_coord_t DisplayManager::getHeight() const {
    if (initialized_) {
        return disp_drv_.ver_res;
    }
    return LV_VER_RES_MAX;
}

void DisplayManager::logOrientation(lv_coord_t width, lv_coord_t height) const {
    Serial.printf("[Display] Orientation -> %s (%dx%d)\n",
                  landscape_ ? "Landscape" : "Portrait",
                  static_cast<int>(width),
                  static_cast<int>(height));
}

lv_obj_t* DisplayManager::getOverlayLayer() {
    if (!initialized_) {
        return nullptr;
    }
    if (!overlay_layer_) {
        overlay_layer_ = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(overlay_layer_);
        lv_obj_set_style_bg_opa(overlay_layer_, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(overlay_layer_, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(overlay_layer_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(overlay_layer_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(overlay_layer_, disp_drv_.hor_res, disp_drv_.ver_res);
        lv_obj_set_pos(overlay_layer_, 0, 0);
    }
    lv_obj_move_foreground(overlay_layer_);
    return overlay_layer_;
}

lv_obj_t* DisplayManager::getLauncherLayer() {
    if (!initialized_) {
        return nullptr;
    }
    if (!launcher_layer_) {
        launcher_layer_ = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(launcher_layer_);
        lv_obj_set_style_bg_opa(launcher_layer_, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(launcher_layer_, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(launcher_layer_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(launcher_layer_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(launcher_layer_, disp_drv_.hor_res, disp_drv_.ver_res);
        lv_obj_set_pos(launcher_layer_, 0, 0);
    }
    lv_obj_move_foreground(launcher_layer_);
    return launcher_layer_;
}

void DisplayManager::refreshOverlayLayer(lv_coord_t width, lv_coord_t height) {
    if (!overlay_layer_) {
        return;
    }
    lv_obj_set_size(overlay_layer_, width, height);
    lv_obj_set_pos(overlay_layer_, 0, 0);
    lv_obj_move_foreground(overlay_layer_);
}

void DisplayManager::refreshLauncherLayer(lv_coord_t width, lv_coord_t height) {
    if (!launcher_layer_) {
        return;
    }
    lv_obj_set_size(launcher_layer_, width, height);
    lv_obj_set_pos(launcher_layer_, 0, 0);
    lv_obj_move_foreground(launcher_layer_);
}

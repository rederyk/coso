#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"

class ThemeSettingsScreen : public Screen {
public:
    ~ThemeSettingsScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void applySnapshot(const SettingsSnapshot& snapshot);
    void updatePreview(const SettingsSnapshot& snapshot);

    static void handlePrimaryColor(lv_event_t* e);
    static void handleAccentColor(lv_event_t* e);
    static void handleBorderRadius(lv_event_t* e);
    static void handleOrientation(lv_event_t* e);
    static void handlePaletteButton(lv_event_t* e);

    lv_obj_t* primary_wheel = nullptr;
    lv_obj_t* accent_wheel = nullptr;
    lv_obj_t* border_slider = nullptr;
    lv_obj_t* orientation_switch = nullptr;
    lv_obj_t* preview_card = nullptr;
    lv_obj_t* preview_header = nullptr;
    lv_obj_t* preview_body = nullptr;

    uint32_t settings_listener_id = 0;
    bool updating_from_manager = false;
};

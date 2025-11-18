#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"

class SettingsScreen : public Screen {
public:
    ~SettingsScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void applySnapshot(const SettingsSnapshot& snapshot);
    void updateBrightnessLabel(uint8_t value);
    size_t themeIndexFromId(const std::string& theme_id) const;
    const char* themeIdFromIndex(size_t index) const;

    static void handleTextInput(lv_event_t* e);
    static void handleBrightnessChanged(lv_event_t* e);
    static void handleThemeChanged(lv_event_t* e);

    lv_obj_t* wifi_ssid_input = nullptr;
    lv_obj_t* wifi_pass_input = nullptr;
    lv_obj_t* brightness_slider = nullptr;
    lv_obj_t* brightness_value_label = nullptr;
    lv_obj_t* theme_dropdown = nullptr;
    lv_obj_t* version_label = nullptr;
    lv_obj_t* hint_label = nullptr;

    uint32_t settings_listener_id = 0;
    bool updating_from_manager = false;
};

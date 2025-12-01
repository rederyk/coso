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
    void applyThemeStyles(const SettingsSnapshot& snapshot);
    void styleCard(lv_obj_t* card, bool allow_half_width, const SettingsSnapshot& snapshot);
    void updateBrightnessLabel(uint8_t value);
    void updateLedBrightnessLabel(uint8_t value);

    static void handleTextInput(lv_event_t* e);
    static void handleBrightnessChanged(lv_event_t* e);
    static void handleLedBrightnessChanged(lv_event_t* e);
    static void handleWifiSettingsButton(lv_event_t* e);
    static void handleBleSettingsButton(lv_event_t* e);
    static void handleLedSettingsButton(lv_event_t* e);
    static void handleVoiceAssistantSettingsButton(lv_event_t* e);
    static void handleDeveloperButton(lv_event_t* e);

    lv_obj_t* wifi_ssid_input = nullptr;
    lv_obj_t* wifi_pass_input = nullptr;
    lv_obj_t* brightness_slider = nullptr;
    lv_obj_t* brightness_value_label = nullptr;
    lv_obj_t* led_brightness_slider = nullptr;
    lv_obj_t* led_brightness_value_label = nullptr;
    lv_obj_t* header_label = nullptr;
    lv_obj_t* content_container = nullptr;
    lv_obj_t* connectivity_card = nullptr;
    lv_obj_t* display_card = nullptr;
    lv_obj_t* led_card = nullptr;
    lv_obj_t* info_card = nullptr;
    lv_obj_t* version_label = nullptr;
    lv_obj_t* hint_label = nullptr;

    uint32_t settings_listener_id = 0;
    bool updating_from_manager = false;
};

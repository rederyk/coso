#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"

class VoiceAssistantSettingsScreen : public Screen {
public:
    // Fixed card height constant for consistent layout
    static constexpr lv_coord_t CARD_HEIGHT_PX = 80;

    ~VoiceAssistantSettingsScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void applySnapshot(const SettingsSnapshot& snapshot);
    void applyThemeStyles(const SettingsSnapshot& snapshot);

    static void handleTriggerAssistantButton(lv_event_t* e);
    static void handleApiKeyInput(lv_event_t* e);
    static void handleEndpointInput(lv_event_t* e);
    static void handleEnabledSwitch(lv_event_t* e);

    lv_obj_t* title_label = nullptr;
    lv_obj_t* trigger_card = nullptr;
    lv_obj_t* enabled_card = nullptr;
    lv_obj_t* api_card = nullptr;
    lv_obj_t* endpoint_card = nullptr;
    lv_obj_t* trigger_button = nullptr;
    lv_obj_t* enabled_switch = nullptr;
    lv_obj_t* api_key_input = nullptr;
    lv_obj_t* endpoint_input = nullptr;
    lv_obj_t* api_key_hint = nullptr;
    lv_obj_t* endpoint_hint = nullptr;

    uint32_t settings_listener_id = 0;
    bool updating_from_manager = false;
};

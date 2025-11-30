#pragma once

#include "core/screen.h"
#include <lvgl.h>

/**
 * Audio Effects Screen
 * Controls for EQ, reverb, echo and presets
 */
class AudioEffectsScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void updateEffectsState();
    void applyPreset(const char* preset_name);

    static void onEqEnableToggled(lv_event_t* event);
    static void onEqBandChanged(lv_event_t* event);
    static void onPresetSelected(lv_event_t* event);
    static void onReverbEnableToggled(lv_event_t* event);
    static void onReverbParamChanged(lv_event_t* event);
    static void onEchoEnableToggled(lv_event_t* event);
    static void onEchoParamChanged(lv_event_t* event);

    // UI Elements - Equalizer
    lv_obj_t* eq_enable_switch = nullptr;
    lv_obj_t* eq_sliders[5] = {nullptr};
    lv_obj_t* eq_labels[5] = {nullptr};
    lv_obj_t* preset_dropdown = nullptr;

    // UI Elements - Reverb
    lv_obj_t* reverb_enable_switch = nullptr;
    lv_obj_t* reverb_room_slider = nullptr;
    lv_obj_t* reverb_damp_slider = nullptr;
    lv_obj_t* reverb_mix_slider = nullptr;

    // UI Elements - Echo
    lv_obj_t* echo_enable_switch = nullptr;
    lv_obj_t* echo_delay_slider = nullptr;
    lv_obj_t* echo_feedback_slider = nullptr;
    lv_obj_t* echo_mix_slider = nullptr;
};

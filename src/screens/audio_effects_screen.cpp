#include "screens/audio_effects_screen.h"
#include "core/audio_manager.h"
#include "core/settings_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/color_utils.h"

namespace {
    constexpr const char* EQ_BAND_NAMES[] = {"Bass", "Low-Mid", "Mid", "High-Mid", "Treble"};

    lv_obj_t* create_card(lv_obj_t* parent, const char* title, lv_color_t bg_color) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(card, bg_color, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_pad_all(card, 12, 0);
        lv_obj_set_layout(card, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 8, 0);

        lv_obj_t* title_label = lv_label_create(card);
        lv_label_set_text(title_label, title);
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(title_label, ColorUtils::invertColor(bg_color), 0);

        return card;
    }

    lv_obj_t* create_slider_row(lv_obj_t* parent, const char* label, int min, int max, int value, lv_event_cb_t cb, void* user_data) {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* label_obj = lv_label_create(row);
        lv_label_set_text(label_obj, label);
        lv_obj_set_width(label_obj, 70);

        lv_obj_t* slider = lv_slider_create(row);
        lv_obj_set_flex_grow(slider, 1);
        lv_slider_set_range(slider, min, max);
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
        lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, user_data);

        return slider;
    }
}

void AudioEffectsScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    auto& settings = SettingsManager::getInstance().getSnapshot();
    lv_color_t theme_color = lv_color_hex(settings.primaryColor);
    lv_color_t bg_color = lv_color_hex(settings.cardColor);
    lv_color_t text_color = ColorUtils::invertColor(bg_color);

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_set_style_pad_row(root, 8, 0);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);

    // Header
    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text(header, LV_SYMBOL_AUDIO " Audio Effects");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(header, text_color, 0);

    // ========== EQUALIZER CARD ==========
    lv_obj_t* eq_card = create_card(root, LV_SYMBOL_SETTINGS " Equalizer", theme_color);

    // EQ Enable switch
    lv_obj_t* eq_switch_row = lv_obj_create(eq_card);
    lv_obj_remove_style_all(eq_switch_row);
    lv_obj_set_width(eq_switch_row, lv_pct(100));
    lv_obj_set_height(eq_switch_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(eq_switch_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(eq_switch_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(eq_switch_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* eq_enable_label = lv_label_create(eq_switch_row);
    lv_label_set_text(eq_enable_label, "Enable");

    eq_enable_switch = lv_switch_create(eq_switch_row);
    lv_obj_add_event_cb(eq_enable_switch, onEqEnableToggled, LV_EVENT_VALUE_CHANGED, this);

    // Preset dropdown
    lv_obj_t* preset_label = lv_label_create(eq_card);
    lv_label_set_text(preset_label, "Preset:");

    preset_dropdown = lv_dropdown_create(eq_card);
    lv_obj_set_width(preset_dropdown, lv_pct(100));
    lv_dropdown_set_options(preset_dropdown, "Flat\nRock\nJazz\nClassical\nPop\nBass Boost\nTreble Boost\nVocal");
    lv_obj_add_event_cb(preset_dropdown, onPresetSelected, LV_EVENT_VALUE_CHANGED, this);

    // EQ Band sliders
    for (int i = 0; i < 5; ++i) {
        eq_sliders[i] = create_slider_row(eq_card, EQ_BAND_NAMES[i], -12, 12, 0, onEqBandChanged, this);
        lv_obj_set_user_data(eq_sliders[i], reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_set_style_bg_color(eq_sliders[i], theme_color, LV_PART_INDICATOR);
    }

    // ========== REVERB CARD ==========
    lv_obj_t* reverb_card = create_card(root, LV_SYMBOL_CHARGE " Reverb", lv_color_hex(0x16213e));

    // Reverb enable switch
    lv_obj_t* reverb_switch_row = lv_obj_create(reverb_card);
    lv_obj_remove_style_all(reverb_switch_row);
    lv_obj_set_width(reverb_switch_row, lv_pct(100));
    lv_obj_set_height(reverb_switch_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(reverb_switch_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(reverb_switch_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(reverb_switch_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* reverb_enable_label = lv_label_create(reverb_switch_row);
    lv_label_set_text(reverb_enable_label, "Enable");

    reverb_enable_switch = lv_switch_create(reverb_switch_row);
    lv_obj_add_event_cb(reverb_enable_switch, onReverbEnableToggled, LV_EVENT_VALUE_CHANGED, this);

    // Reverb parameters
    reverb_room_slider = create_slider_row(reverb_card, "Room Size", 0, 100, 50, onReverbParamChanged, this);
    lv_obj_set_user_data(reverb_room_slider, reinterpret_cast<void*>(0));

    reverb_damp_slider = create_slider_row(reverb_card, "Damping", 0, 100, 50, onReverbParamChanged, this);
    lv_obj_set_user_data(reverb_damp_slider, reinterpret_cast<void*>(1));

    reverb_mix_slider = create_slider_row(reverb_card, "Mix", 0, 100, 30, onReverbParamChanged, this);
    lv_obj_set_user_data(reverb_mix_slider, reinterpret_cast<void*>(2));

    // ========== ECHO CARD ==========
    lv_obj_t* echo_card = create_card(root, LV_SYMBOL_LOOP " Echo", lv_color_hex(0x0f3460));

    // Echo enable switch
    lv_obj_t* echo_switch_row = lv_obj_create(echo_card);
    lv_obj_remove_style_all(echo_switch_row);
    lv_obj_set_width(echo_switch_row, lv_pct(100));
    lv_obj_set_height(echo_switch_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(echo_switch_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(echo_switch_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(echo_switch_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* echo_enable_label = lv_label_create(echo_switch_row);
    lv_label_set_text(echo_enable_label, "Enable");

    echo_enable_switch = lv_switch_create(echo_switch_row);
    lv_obj_add_event_cb(echo_enable_switch, onEchoEnableToggled, LV_EVENT_VALUE_CHANGED, this);

    // Echo parameters
    echo_delay_slider = create_slider_row(echo_card, "Delay (ms)", 50, 1000, 300, onEchoParamChanged, this);
    lv_obj_set_user_data(echo_delay_slider, reinterpret_cast<void*>(0));

    echo_feedback_slider = create_slider_row(echo_card, "Feedback", 0, 90, 40, onEchoParamChanged, this);
    lv_obj_set_user_data(echo_feedback_slider, reinterpret_cast<void*>(1));

    echo_mix_slider = create_slider_row(echo_card, "Mix", 0, 100, 30, onEchoParamChanged, this);
    lv_obj_set_user_data(echo_mix_slider, reinterpret_cast<void*>(2));
}

void AudioEffectsScreen::onShow() {
    updateEffectsState();
}

void AudioEffectsScreen::onHide() {
    // Nothing to cleanup
}

void AudioEffectsScreen::updateEffectsState() {
    auto* effects = AudioManager::getInstance().getEffectsChain();
    if (!effects) return;

    // Update EQ state
    auto* eq = effects->getEqualizer();
    if (eq) {
        lv_obj_add_state(eq_enable_switch, eq->isEnabled() ? LV_STATE_CHECKED : 0);

        for (int i = 0; i < 5; ++i) {
            float gain = eq->getBandGain(static_cast<EqualizerEffect::Band>(i));
            lv_slider_set_value(eq_sliders[i], static_cast<int>(gain), LV_ANIM_OFF);
        }
    }

    // Update Reverb state
    auto* reverb = effects->getReverb();
    if (reverb) {
        lv_obj_add_state(reverb_enable_switch, reverb->isEnabled() ? LV_STATE_CHECKED : 0);
        lv_slider_set_value(reverb_room_slider, static_cast<int>(reverb->getRoomSize() * 100), LV_ANIM_OFF);
        lv_slider_set_value(reverb_damp_slider, static_cast<int>(reverb->getDamping() * 100), LV_ANIM_OFF);
        lv_slider_set_value(reverb_mix_slider, static_cast<int>(reverb->getWetMix() * 100), LV_ANIM_OFF);
    }

    // Update Echo state
    auto* echo = effects->getEcho();
    if (echo) {
        lv_obj_add_state(echo_enable_switch, echo->isEnabled() ? LV_STATE_CHECKED : 0);
        lv_slider_set_value(echo_delay_slider, static_cast<int>(echo->getDelayTime()), LV_ANIM_OFF);
        lv_slider_set_value(echo_feedback_slider, static_cast<int>(echo->getFeedback() * 100), LV_ANIM_OFF);
        lv_slider_set_value(echo_mix_slider, static_cast<int>(echo->getWetMix() * 100), LV_ANIM_OFF);
    }
}

void AudioEffectsScreen::applyPreset(const char* preset_name) {
    auto* effects = AudioManager::getInstance().getEffectsChain();
    if (!effects) return;

    auto* eq = effects->getEqualizer();
    if (eq) {
        eq->applyPreset(preset_name);

        // Update sliders
        for (int i = 0; i < 5; ++i) {
            float gain = eq->getBandGain(static_cast<EqualizerEffect::Band>(i));
            lv_slider_set_value(eq_sliders[i], static_cast<int>(gain), LV_ANIM_ON);
        }

        Logger::getInstance().infof("[Effects] Applied preset: %s", preset_name);
    }
}

// Event callbacks
void AudioEffectsScreen::onEqEnableToggled(lv_event_t* event) {
    auto* effects = AudioManager::getInstance().getEffectsChain();
    if (!effects) return;

    auto* eq = effects->getEqualizer();
    if (eq) {
        lv_obj_t* sw = lv_event_get_target(event);
        bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        eq->setEnabled(enabled);
        Logger::getInstance().infof("[Effects] EQ %s", enabled ? "enabled" : "disabled");
    }
}

void AudioEffectsScreen::onEqBandChanged(lv_event_t* event) {
    auto* effects = AudioManager::getInstance().getEffectsChain();
    if (!effects) return;

    auto* eq = effects->getEqualizer();
    if (eq) {
        lv_obj_t* slider = lv_event_get_target(event);
        int band_idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(slider)));
        int value = lv_slider_get_value(slider);

        eq->setBandGain(static_cast<EqualizerEffect::Band>(band_idx), static_cast<float>(value));
    }
}

void AudioEffectsScreen::onPresetSelected(lv_event_t* event) {
    AudioEffectsScreen* screen = static_cast<AudioEffectsScreen*>(lv_event_get_user_data(event));
    lv_obj_t* dd = lv_event_get_target(event);

    char buf[32];
    lv_dropdown_get_selected_str(dd, buf, sizeof(buf));

    screen->applyPreset(buf);
}

void AudioEffectsScreen::onReverbEnableToggled(lv_event_t* event) {
    auto* effects = AudioManager::getInstance().getEffectsChain();
    if (!effects) return;

    auto* reverb = effects->getReverb();
    if (reverb) {
        lv_obj_t* sw = lv_event_get_target(event);
        bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        reverb->setEnabled(enabled);
        Logger::getInstance().infof("[Effects] Reverb %s", enabled ? "enabled" : "disabled");
    }
}

void AudioEffectsScreen::onReverbParamChanged(lv_event_t* event) {
    auto* effects = AudioManager::getInstance().getEffectsChain();
    if (!effects) return;

    auto* reverb = effects->getReverb();
    if (reverb) {
        lv_obj_t* slider = lv_event_get_target(event);
        int param_idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(slider)));
        int value = lv_slider_get_value(slider);

        switch (param_idx) {
            case 0: reverb->setRoomSize(value / 100.0f); break;
            case 1: reverb->setDamping(value / 100.0f); break;
            case 2: reverb->setWetMix(value / 100.0f); break;
        }
    }
}

void AudioEffectsScreen::onEchoEnableToggled(lv_event_t* event) {
    auto* effects = AudioManager::getInstance().getEffectsChain();
    if (!effects) return;

    auto* echo = effects->getEcho();
    if (echo) {
        lv_obj_t* sw = lv_event_get_target(event);
        bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        echo->setEnabled(enabled);
        Logger::getInstance().infof("[Effects] Echo %s", enabled ? "enabled" : "disabled");
    }
}

void AudioEffectsScreen::onEchoParamChanged(lv_event_t* event) {
    auto* effects = AudioManager::getInstance().getEffectsChain();
    if (!effects) return;

    auto* echo = effects->getEcho();
    if (echo) {
        lv_obj_t* slider = lv_event_get_target(event);
        int param_idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(slider)));
        int value = lv_slider_get_value(slider);

        switch (param_idx) {
            case 0: echo->setDelayTime(static_cast<float>(value)); break;
            case 1: echo->setFeedback(value / 100.0f); break;
            case 2: echo->setWetMix(value / 100.0f); break;
        }
    }
}

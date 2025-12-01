#include "screens/voice_assistant_settings_screen.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/color_utils.h"
#include "core/voice_assistant.h"
#include "core/app_manager.h"

namespace {
lv_obj_t* create_fixed_card(lv_obj_t* parent, const char* title, lv_color_t bg_color = lv_color_hex(0x10182c)) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, 80);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, bg_color, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_outline_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 4, 0);

    lv_obj_t* header = lv_label_create(card);
    lv_label_set_text(header, title);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
    // Invert text color based on background
    lv_color_t text_color = ColorUtils::invertColor(bg_color);
    lv_obj_set_style_text_color(header, text_color, 0);

    return card;
}
} // namespace

VoiceAssistantSettingsScreen::~VoiceAssistantSettingsScreen() {
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void VoiceAssistantSettingsScreen::build(lv_obj_t* parent) {
    if (!parent) {
        return;
    }

    SettingsManager& manager = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = manager.getSnapshot();

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x040b18), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_set_style_pad_row(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_outline_width(root, 0, 0);

    title_label = lv_label_create(root);
    lv_label_set_text(title_label, LV_SYMBOL_AUDIO " Voice Assistant");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_width(title_label, lv_pct(100));

    // Trigger assistant button card
    trigger_card = create_fixed_card(root, "Trigger Assistant");
    trigger_button = lv_btn_create(trigger_card);
    lv_obj_set_height(trigger_button, 50);
    lv_obj_add_event_cb(trigger_button, handleTriggerAssistantButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(trigger_button, lv_color_hex(0x00aa44), 0); // Green trigger button
    lv_obj_t* trigger_btn_label = lv_label_create(trigger_button);
    lv_label_set_text(trigger_btn_label, LV_SYMBOL_AUDIO " Skip Activation & Listen");
    lv_obj_center(trigger_btn_label);
    lv_obj_set_style_text_font(trigger_btn_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(trigger_btn_label, lv_color_hex(0xffffff), 0);

    // Enabled switch card
    enabled_card = create_fixed_card(root, "Voice Assistant Enabled");
    enabled_switch = lv_switch_create(enabled_card);
    lv_obj_add_event_cb(enabled_switch, handleEnabledSwitch, LV_EVENT_VALUE_CHANGED, this);

    // API Key input card
    api_card = create_fixed_card(root, "OpenAI API Key");
    api_key_input = lv_textarea_create(api_card);
    lv_textarea_set_placeholder_text(api_key_input, "Enter your OpenAI API key...");
    lv_textarea_set_password_mode(api_key_input, true);
    lv_textarea_set_one_line(api_key_input, true);
    lv_obj_set_width(api_key_input, lv_pct(100));
    lv_obj_add_event_cb(api_key_input, handleApiKeyInput, LV_EVENT_READY, this);
    api_key_hint = lv_label_create(api_card);
    lv_label_set_text(api_key_hint, "Required for Whisper and GPT APIs");
    lv_obj_set_style_text_font(api_key_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(api_key_hint, lv_color_hex(0xa0a0a0), 0);

    // Endpoint input card
    endpoint_card = create_fixed_card(root, "API Endpoint");
    endpoint_input = lv_textarea_create(endpoint_card);
    lv_textarea_set_placeholder_text(endpoint_input, "https://api.openai.com/v1");
    lv_textarea_set_one_line(endpoint_input, true);
    lv_obj_set_width(endpoint_input, lv_pct(100));
    lv_obj_add_event_cb(endpoint_input, handleEndpointInput, LV_EVENT_READY, this);
    endpoint_hint = lv_label_create(endpoint_card);
    lv_label_set_text(endpoint_hint, "Usually default OpenAI endpoint");
    lv_obj_set_style_text_font(endpoint_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(endpoint_hint, lv_color_hex(0xa0a0a0), 0);

    applySnapshot(snapshot);

    if (settings_listener_id == 0) {
        settings_listener_id = manager.addListener([this](SettingsManager::SettingKey, const SettingsSnapshot& snap) {
            if (!root) {
                return;
            }
            applySnapshot(snap);
        });
    }
}

void VoiceAssistantSettingsScreen::onShow() {
    Logger::getInstance().info(LV_SYMBOL_AUDIO " Voice assistant settings screen shown");
    applySnapshot(SettingsManager::getInstance().getSnapshot());
}

void VoiceAssistantSettingsScreen::onHide() {
    Logger::getInstance().info(LV_SYMBOL_AUDIO " Voice assistant settings screen hidden");
}

void VoiceAssistantSettingsScreen::applySnapshot(const SettingsSnapshot& snapshot) {
    updating_from_manager = true;

    if (enabled_switch) {
        lv_obj_clear_state(enabled_switch, LV_STATE_CHECKED);
        if (snapshot.voiceAssistantEnabled) {
            lv_obj_add_state(enabled_switch, LV_STATE_CHECKED);
        }
    }
    if (api_key_input) {
        lv_textarea_set_text(api_key_input, snapshot.openAiApiKey.c_str());
    }
    if (endpoint_input) {
        lv_textarea_set_text(endpoint_input, snapshot.openAiEndpoint.c_str());
    }

    applyThemeStyles(snapshot);
    updating_from_manager = false;
}

void VoiceAssistantSettingsScreen::applyThemeStyles(const SettingsSnapshot& snapshot) {
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
    }
    if (title_label) {
        lv_obj_set_style_text_color(title_label, accent, 0);
    }

    // Style all cards uniformly
    auto style_card = [&](lv_obj_t* card) {
        if (!card) return;
        lv_color_t card_color = lv_color_hex(snapshot.cardColor);
        lv_obj_set_style_bg_color(card, card_color, 0);
        lv_obj_set_style_radius(card, snapshot.borderRadius, 0);

        // Update header text color in cards
        uint32_t child_count = lv_obj_get_child_cnt(card);
        for (uint32_t i = 0; i < child_count; ++i) {
            lv_obj_t* child = lv_obj_get_child(card, i);
            if (child && lv_obj_check_type(child, &lv_label_class)) {
                const lv_font_t* font = lv_obj_get_style_text_font(child, 0);
                if (font == &lv_font_montserrat_16) {
                    // Header label
                    lv_color_t header_color = ColorUtils::invertColor(card_color);
                    lv_obj_set_style_text_color(child, header_color, 0);
                } else {
                    // Hint label
                    lv_obj_set_style_text_color(child, lv_color_mix(accent, lv_color_hex(0xffffff), LV_OPA_40), 0);
                }
            }
        }
    };

    style_card(trigger_card);
    style_card(enabled_card);
    style_card(api_card);
    style_card(endpoint_card);

    if (trigger_button) {
        lv_obj_set_style_bg_color(trigger_button, accent, 0);
        lv_obj_set_style_border_color(trigger_button, accent, 0);
    }
    if (enabled_switch) {
        lv_color_t switch_bg = lv_color_mix(lv_color_hex(snapshot.dockColor), primary, LV_OPA_50);
        lv_obj_set_style_bg_color(enabled_switch, switch_bg, LV_PART_MAIN);
        lv_obj_set_style_bg_color(enabled_switch, accent, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(enabled_switch, lv_color_hex(0xffffff), LV_PART_KNOB);
    }
}

void VoiceAssistantSettingsScreen::handleTriggerAssistantButton(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    VoiceAssistant::getInstance().triggerListening();
    Logger::getInstance().info(LV_SYMBOL_AUDIO " Manual voice assistant trigger initiated");
}

void VoiceAssistantSettingsScreen::handleApiKeyInput(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* textarea = lv_event_get_target(e);
    const char* text = lv_textarea_get_text(textarea);
    if (text) {
        SettingsManager::getInstance().setOpenAiApiKey(text);
    }
}

void VoiceAssistantSettingsScreen::handleEndpointInput(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* textarea = lv_event_get_target(e);
    const char* text = lv_textarea_get_text(textarea);
    if (text) {
        SettingsManager::getInstance().setOpenAiEndpoint(text);
    }
}

void VoiceAssistantSettingsScreen::handleEnabledSwitch(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* switch_obj = lv_event_get_target(e);
    bool checked = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
    SettingsManager::getInstance().setVoiceAssistantEnabled(checked);
    Logger::getInstance().infof(LV_SYMBOL_AUDIO " Voice assistant %s", checked ? "enabled" : "disabled");
}

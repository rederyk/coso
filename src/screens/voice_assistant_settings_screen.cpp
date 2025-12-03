#include "screens/voice_assistant_settings_screen.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/color_utils.h"
#include "core/voice_assistant.h"
#include "core/app_manager.h"
#include "core/keyboard_manager.h"

// Forward declaration to avoid include issues
class VoiceAssistant;

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
    trigger_card = create_fixed_card(root, "Voice Assistant");
    trigger_button = lv_btn_create(trigger_card);
    lv_obj_set_height(trigger_button, 50);
    lv_obj_add_event_cb(trigger_button, handleTriggerPressed, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(trigger_button, handleTriggerReleased, LV_EVENT_RELEASED, this);
    lv_obj_set_style_bg_color(trigger_button, lv_color_hex(0x00aa44), 0); // Green trigger button
    lv_obj_set_style_bg_color(trigger_button, lv_color_hex(0xaa0000), LV_STATE_PRESSED); // Red when pressed
    trigger_btn_label = lv_label_create(trigger_button);
    lv_label_set_text(trigger_btn_label, LV_SYMBOL_AUDIO " Hold to Talk");
    lv_obj_center(trigger_btn_label);
    lv_obj_set_style_text_font(trigger_btn_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(trigger_btn_label, lv_color_hex(0xffffff), 0);

    // Enabled switch card
    enabled_card = create_fixed_card(root, "Voice Assistant Enabled");
    enabled_switch = lv_switch_create(enabled_card);
    lv_obj_add_event_cb(enabled_switch, handleEnabledSwitch, LV_EVENT_VALUE_CHANGED, this);

    // Local API Mode switch card
    local_mode_card = create_fixed_card(root, "Use Local APIs (Docker)");
    local_mode_switch = lv_switch_create(local_mode_card);
    lv_obj_add_event_cb(local_mode_switch, handleLocalModeSwitch, LV_EVENT_VALUE_CHANGED, this);

    // API Key input card
    api_card = create_fixed_card(root, "OpenAI API Key");
    api_key_input = lv_textarea_create(api_card);
    lv_textarea_set_placeholder_text(api_key_input, "Enter your OpenAI API key...");
    lv_textarea_set_password_mode(api_key_input, true);
    lv_textarea_set_one_line(api_key_input, true);
    lv_obj_set_width(api_key_input, lv_pct(100));
    lv_obj_add_event_cb(api_key_input, handleTextAreaFocused, LV_EVENT_FOCUSED, this);
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
    lv_obj_add_event_cb(endpoint_input, handleTextAreaFocused, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(endpoint_input, handleEndpointInput, LV_EVENT_READY, this);
    endpoint_hint = lv_label_create(endpoint_card);
    lv_label_set_text(endpoint_hint, "Usually default OpenAI endpoint");
    lv_obj_set_style_text_font(endpoint_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(endpoint_hint, lv_color_hex(0xa0a0a0), 0);

    // Whisper STT Endpoint card
    whisper_card = create_fixed_card(root, "Whisper STT Endpoint");
    whisper_endpoint_input = lv_textarea_create(whisper_card);
    lv_textarea_set_placeholder_text(whisper_endpoint_input, "http://192.168.1.51:8002/v1/audio/transcriptions");
    lv_textarea_set_one_line(whisper_endpoint_input, true);
    lv_obj_set_width(whisper_endpoint_input, lv_pct(100));
    lv_obj_add_event_cb(whisper_endpoint_input, handleTextAreaFocused, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(whisper_endpoint_input, handleWhisperEndpointInput, LV_EVENT_READY, this);
    whisper_hint = lv_label_create(whisper_card);
    lv_label_set_text(whisper_hint, "Cloud or Local Whisper endpoint");
    lv_obj_set_style_text_font(whisper_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(whisper_hint, lv_color_hex(0xa0a0a0), 0);

    // LLM Endpoint card
    llm_card = create_fixed_card(root, "LLM Endpoint");
    llm_endpoint_input = lv_textarea_create(llm_card);
    lv_textarea_set_placeholder_text(llm_endpoint_input, "http://192.168.1.51:11434/v1/chat/completions");
    lv_textarea_set_one_line(llm_endpoint_input, true);
    lv_obj_set_width(llm_endpoint_input, lv_pct(100));
    lv_obj_add_event_cb(llm_endpoint_input, handleTextAreaFocused, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(llm_endpoint_input, handleLlmEndpointInput, LV_EVENT_READY, this);
    llm_hint = lv_label_create(llm_card);
    lv_label_set_text(llm_hint, "Cloud or Local LLM endpoint");
    lv_obj_set_style_text_font(llm_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(llm_hint, lv_color_hex(0xa0a0a0), 0);

    // LLM Model card
    llm_model_card = create_fixed_card(root, "LLM Model");
    llm_model_input = lv_textarea_create(llm_model_card);
    lv_textarea_set_placeholder_text(llm_model_input, "llama3.2:3b");
    lv_textarea_set_one_line(llm_model_input, true);
    lv_obj_set_width(llm_model_input, lv_pct(100));
    lv_obj_add_event_cb(llm_model_input, handleTextAreaFocused, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(llm_model_input, handleLlmModelInput, LV_EVENT_READY, this);
    llm_model_hint = lv_label_create(llm_model_card);
    lv_label_set_text(llm_model_hint, "Model name for LLM requests");
    lv_obj_set_style_text_font(llm_model_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(llm_model_hint, lv_color_hex(0xa0a0a0), 0);

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

    // Initialize VoiceAssistant if not already done
    // Note: We do this here because VoiceAssistant is not initialized at system startup
    static bool voice_assistant_initialized = false;
    if (!voice_assistant_initialized) {
        Logger::getInstance().info("[VoiceAssistant] Initializing voice assistant from settings screen");
        if (VoiceAssistant::getInstance().begin()) {
            Logger::getInstance().info("[VoiceAssistant] Voice assistant initialized successfully");
            voice_assistant_initialized = true;
        } else {
            Logger::getInstance().warn("[VoiceAssistant] Voice assistant initialization failed");
        }
    }

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
    if (local_mode_switch) {
        lv_obj_clear_state(local_mode_switch, LV_STATE_CHECKED);
        if (snapshot.localApiMode) {
            lv_obj_add_state(local_mode_switch, LV_STATE_CHECKED);
        }
    }
    if (api_key_input) {
        lv_textarea_set_text(api_key_input, snapshot.openAiApiKey.c_str());
    }
    if (endpoint_input) {
        lv_textarea_set_text(endpoint_input, snapshot.openAiEndpoint.c_str());
    }
    if (whisper_endpoint_input) {
        // Show appropriate endpoint based on local mode
        std::string endpoint = snapshot.localApiMode ? snapshot.whisperLocalEndpoint : snapshot.whisperCloudEndpoint;
        lv_textarea_set_text(whisper_endpoint_input, endpoint.c_str());
    }
    if (llm_endpoint_input) {
        // Show appropriate endpoint based on local mode
        std::string endpoint = snapshot.localApiMode ? snapshot.llmLocalEndpoint : snapshot.llmCloudEndpoint;
        lv_textarea_set_text(llm_endpoint_input, endpoint.c_str());
    }
    if (llm_model_input) {
        lv_textarea_set_text(llm_model_input, snapshot.llmModel.c_str());
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
    style_card(local_mode_card);
    style_card(api_card);
    style_card(endpoint_card);
    style_card(whisper_card);
    style_card(llm_card);
    style_card(llm_model_card);

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

void VoiceAssistantSettingsScreen::handleTriggerPressed(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    // Start recording
    VoiceAssistant::getInstance().startRecording();
    Logger::getInstance().info(LV_SYMBOL_AUDIO " Voice recording started");

    // Update button text to show recording state
    if (screen->trigger_btn_label) {
        lv_label_set_text(screen->trigger_btn_label, LV_SYMBOL_AUDIO " Recording... (Release to Process)");
    }
}

void VoiceAssistantSettingsScreen::handleTriggerReleased(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    // Stop recording and process
    VoiceAssistant::getInstance().stopRecordingAndProcess();
    Logger::getInstance().info(LV_SYMBOL_AUDIO " Voice recording stopped, processing...");

    // Reset button text
    if (screen->trigger_btn_label) {
        lv_label_set_text(screen->trigger_btn_label, LV_SYMBOL_AUDIO " Hold to Talk");
    }
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

    // If enabling, try to initialize VoiceAssistant now
    if (checked) {
        static bool voice_assistant_initialized = false;
        if (!voice_assistant_initialized) {
            Logger::getInstance().info("[VoiceAssistant] Initializing voice assistant after enable");
            if (VoiceAssistant::getInstance().begin()) {
                Logger::getInstance().info("[VoiceAssistant] Voice assistant initialized successfully");
                voice_assistant_initialized = true;
            } else {
                Logger::getInstance().warn("[VoiceAssistant] Voice assistant initialization failed");
            }
        }
    }
}

void VoiceAssistantSettingsScreen::handleLocalModeSwitch(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* switch_obj = lv_event_get_target(e);
    bool checked = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
    SettingsManager::getInstance().setLocalApiMode(checked);
    Logger::getInstance().infof(LV_SYMBOL_AUDIO " Local API mode %s", checked ? "enabled" : "disabled");

    // Update endpoint displays to show appropriate values
    screen->applySnapshot(SettingsManager::getInstance().getSnapshot());
}

void VoiceAssistantSettingsScreen::handleWhisperEndpointInput(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* textarea = lv_event_get_target(e);
    const char* text = lv_textarea_get_text(textarea);
    if (text) {
        const SettingsSnapshot& snapshot = SettingsManager::getInstance().getSnapshot();
        if (snapshot.localApiMode) {
            SettingsManager::getInstance().setWhisperLocalEndpoint(text);
        } else {
            SettingsManager::getInstance().setWhisperCloudEndpoint(text);
        }
    }
}

void VoiceAssistantSettingsScreen::handleLlmEndpointInput(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* textarea = lv_event_get_target(e);
    const char* text = lv_textarea_get_text(textarea);
    if (text) {
        const SettingsSnapshot& snapshot = SettingsManager::getInstance().getSnapshot();
        if (snapshot.localApiMode) {
            SettingsManager::getInstance().setLlmLocalEndpoint(text);
        } else {
            SettingsManager::getInstance().setLlmCloudEndpoint(text);
        }
    }
}

void VoiceAssistantSettingsScreen::handleLlmModelInput(lv_event_t* e) {
    auto* screen = static_cast<VoiceAssistantSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* textarea = lv_event_get_target(e);
    const char* text = lv_textarea_get_text(textarea);
    if (text) {
        SettingsManager::getInstance().setLlmModel(text);
    }
}

void VoiceAssistantSettingsScreen::handleTextAreaFocused(lv_event_t* e) {
    lv_obj_t* textarea = lv_event_get_target(e);
    if (!textarea) return;

    // Show keyboard when text area is focused
    KeyboardManager::getInstance().showForTextArea(textarea, [textarea](const char* text) {
        // When user presses OK/Enter, trigger the READY event to save the value
        if (text) {
            lv_event_send(textarea, LV_EVENT_READY, nullptr);
        }
    });
}

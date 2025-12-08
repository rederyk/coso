#include "screens/ai_chat_screen.h"
#include "core/async_request_manager.h"
#include "core/conversation_buffer.h"
#include "core/voice_assistant.h"
#include "core/wifi_manager.h"
#include "core/ble_hid_manager.h"
#include "core/audio_manager.h"
#include "core/keyboard_manager.h"
#include "drivers/sd_card_driver.h"
#include "core/app_manager.h"
#include "core/display_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_log.h>

static const char* TAG = "AiChatScreen";

AiChatScreen* AiChatScreen::instance = nullptr;

namespace {
lv_obj_t* create_card(lv_obj_t* parent, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 10, 0);

    if (title) {
        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    }

    return card;
}

lv_obj_t* create_chip_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_pad_all(btn, 6, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn, 4, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}
} // namespace

AiChatScreen::~AiChatScreen() {
    if (status_timer) {
        lv_timer_del(status_timer);
        status_timer = nullptr;
    }
    if (poll_timer) {
        lv_timer_del(poll_timer);
        poll_timer = nullptr;
    }
    if (tts_status_timer) {
        lv_timer_del(tts_status_timer);
        tts_status_timer = nullptr;
    }
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void AiChatScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    instance = this;
    staticInit();

    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    // Load Lua script for TTS
    lua_sandbox_.execute("dofile('/memory/scripts/lvgl_tts_chat.lua')");

    // Set initial auto_tts_enabled from settings
    auto_tts_enabled = settings.getTtsEnabled();

    // Ensure VoiceAssistant is initialized if enabled
    if (snapshot.voiceAssistantEnabled && !VoiceAssistant::getInstance().isInitialized()) {
        if (!VoiceAssistant::getInstance().begin()) {
            log_e("[AiChatScreen] Failed to initialize VoiceAssistant");
        }
    }

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_pad_row(root, 10, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);

    // Header container
    header_container = lv_obj_create(root);  // Add to h
    lv_obj_remove_style_all(header_container);
    lv_obj_set_width(header_container, lv_pct(100));
    lv_obj_set_height(header_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(header_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header_container, 4, 0);
    lv_obj_set_style_pad_column(header_container, 8, 0);

    header_label = lv_label_create(header_container);
    lv_label_set_text(header_label, "\xEF\x87\xA4 AI Chat Assistant");  // Add mic icon
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_20, 0);

    lv_obj_t* spacer = lv_obj_create(header_container);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);

    status_chip = lv_obj_create(header_container);  // Reuse status_chip concept
    lv_obj_set_size(status_chip, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_radius(status_chip, 8, 0);
    lv_obj_set_style_pad_all(status_chip, 6, 0);
    status_label = lv_label_create(status_chip);  // Reuse status_label for chip text
    lv_label_set_text(status_label, "Pronto");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);

    // Content container
    content_container = lv_obj_create(root);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_width(content_container, lv_pct(100));
    lv_obj_set_height(content_container, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_pad_row(content_container, 10, 0);
    lv_obj_set_style_pad_column(content_container, 8, 0);
    lv_obj_set_layout(content_container, LV_LAYOUT_FLEX);

    
    // Chat card
    chat_card = create_card(content_container, "Conversazione");
    chat_container = lv_obj_create(chat_card);
    lv_obj_set_size(chat_container, lv_pct(100), LV_SIZE_CONTENT);  // Dynamic height based on content
    lv_obj_set_style_pad_all(chat_container, 10, 0);
    lv_obj_set_style_border_width(chat_container, 0, 0);
    lv_obj_set_layout(chat_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(chat_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(chat_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    // No internal scrolling - let root handle it

    // Input card
    input_card = create_card(content_container, "Invio Messaggio");
    lv_obj_set_style_pad_row(input_card, 8, 0);

    // Chat input
    chat_input = lv_textarea_create(input_card);
    lv_textarea_set_one_line(chat_input, false);
    lv_textarea_set_placeholder_text(chat_input, "Scrivi un messaggio...");
    lv_textarea_set_cursor_pos(chat_input, 0);
    lv_obj_set_width(chat_input, lv_pct(100));
    lv_obj_set_height(chat_input, 70);
    lv_obj_add_event_cb(chat_input, inputEvent, LV_EVENT_KEY, nullptr);
    lv_obj_add_event_cb(chat_input, inputEvent, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(chat_input, inputEvent, LV_EVENT_DEFOCUSED, nullptr);

    // Actions row
    lv_obj_t* actions_row = lv_obj_create(input_card);
    lv_obj_set_size(actions_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(actions_row, 8, 0);
    lv_obj_set_style_pad_column(actions_row, 8, 0);
    lv_obj_set_layout(actions_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actions_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Send button as chip
    send_button = create_chip_button(actions_row, "Invia", sendButtonEvent, this);

    // Autosend toggle as checkbox
    autosend_checkbox = lv_checkbox_create(actions_row);
    lv_checkbox_set_text_static(autosend_checkbox,"Autosend");
    lv_obj_add_event_cb(autosend_checkbox, autosendEvent, LV_EVENT_VALUE_CHANGED, nullptr);

    // Auto TTS toggle as checkbox
    lv_obj_t* auto_tts_checkbox = lv_checkbox_create(actions_row);
    lv_checkbox_set_text_static(auto_tts_checkbox, "Auto TTS");
    lv_obj_add_event_cb(auto_tts_checkbox, [](lv_event_t* e) {
        AiChatScreen* screen = AiChatScreen::instance;
        if (screen) {
            bool enabled = lv_obj_has_state(e->target, LV_STATE_CHECKED);
            screen->auto_tts_enabled = enabled;
            SettingsManager::getInstance().setTtsEnabled(enabled);
            screen->setStatus("Auto TTS " + String(enabled ? "abilitato" : "disabilitato"), lv_color_hex(0x70FFBA));
        }
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // Set initial state from settings
    if (auto_tts_enabled) {
        lv_obj_add_state(auto_tts_checkbox, LV_STATE_CHECKED);
    }

    // Memory label below
    memory_label = lv_label_create(input_card);
    lv_label_set_text_static(memory_label, "Buffer: 0 / 30");
    lv_obj_align(memory_label, LV_ALIGN_BOTTOM_RIGHT, -8, -8);

    // PTT Container
    ptt_container = lv_obj_create(content_container);
    lv_obj_remove_style_all(ptt_container);
    lv_obj_set_width(ptt_container, lv_pct(100));
    lv_obj_set_height(ptt_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(ptt_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ptt_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(ptt_container, 10, 0);

    // PTT Button as large block
    ptt_button = lv_btn_create(ptt_container);
    lv_obj_set_size(ptt_button, lv_pct(100), 60);
    lv_obj_set_style_radius(ptt_button, 12, 0);
    lv_obj_set_style_pad_all(ptt_button, 16, 0);
    lv_obj_set_style_border_width(ptt_button, 0, 0);

    // Icon and text inside PTT button
    lv_obj_t* ptt_icon = lv_label_create(ptt_button);
    lv_label_set_text(ptt_icon, "\xEF\x87\xA4");  // Mic icon
    lv_obj_set_style_text_font(ptt_icon, &lv_font_montserrat_24, 0);
    lv_obj_center(ptt_icon);

    lv_obj_t* ptt_label = lv_label_create(ptt_button);
    lv_label_set_text_static(ptt_label, "PushToTalk");
    lv_obj_set_style_text_font(ptt_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ptt_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // Add events for pressed and released
    lv_obj_add_event_cb(ptt_button, pttPressedEvent, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(ptt_button, pttReleasedEvent, LV_EVENT_RELEASED, this);

    // Bottom actions container
    actions_bottom_container = lv_obj_create(content_container);
    lv_obj_remove_style_all(actions_bottom_container);
    lv_obj_set_width(actions_bottom_container, lv_pct(100));
    lv_obj_set_height(actions_bottom_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(actions_bottom_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actions_bottom_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(actions_bottom_container, 8, 0);
    lv_obj_set_style_pad_column(actions_bottom_container, 10, 0);

    // Reset buffer button
    reset_buffer_button = create_chip_button(actions_bottom_container, "Reset Buffer", resetBufferEvent, this);

    // Save settings button
    save_settings_button = create_chip_button(actions_bottom_container, "Salva Impostazioni", saveSettingsEvent, this);

    applyTheme(snapshot);
    updateStatusIcons();
    loadConversationHistory();
    updateLayout(snapshot.landscapeLayout);

    // Timer
    status_timer = lv_timer_create(statusUpdateTimer, 3000, this);
    poll_timer = nullptr; // Will be set when polling starts

    // Settings listener
    if (settings_listener_id == 0) {
        settings_listener_id = settings.addListener([this](SettingsManager::SettingKey key, const SettingsSnapshot& snap) {
            switch (key) {
                case SettingsManager::SettingKey::Theme:
                case SettingsManager::SettingKey::ThemePrimaryColor:
                case SettingsManager::SettingKey::ThemeAccentColor:
                case SettingsManager::SettingKey::ThemeBorderRadius:
                case SettingsManager::SettingKey::LayoutOrientation:
                case SettingsManager::SettingKey::VoiceAssistantEnabled:
                case SettingsManager::SettingKey::AutosendEnabled:
                case SettingsManager::SettingKey::TtsEnabled:
                    if (key == SettingsManager::SettingKey::TtsEnabled) {
                        auto_tts_enabled = snap.ttsEnabled;
                    }
                    if (snap.voiceAssistantEnabled && !VoiceAssistant::getInstance().isInitialized()) {
                        if (!VoiceAssistant::getInstance().begin()) {
                            log_e("[AiChatScreen] Failed to initialize VoiceAssistant on settings change");
                        }
                    }
                    if (key == SettingsManager::SettingKey::AutosendEnabled) {
                        autosend_enabled = snap.autosendEnabled;
                        if (autosend_checkbox) {
                            if (autosend_enabled) {
                                lv_obj_add_state(autosend_checkbox, LV_STATE_CHECKED);
                            } else {
                                lv_obj_clear_state(autosend_checkbox, LV_STATE_CHECKED);
                            }
                        }
                    }
                    applyTheme(snap);
                    updateLayout(snap.landscapeLayout);
                    break;
                default:
                    break;
            }
        });
    }

    // Set initial autosend from settings
    autosend_enabled = snapshot.autosendEnabled;
    if (autosend_enabled) {
        lv_obj_add_state(autosend_checkbox, LV_STATE_CHECKED);
    }
    current_request_id = "";
    polling_active = false;
}

void AiChatScreen::onShow() {
    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();
    
    // Update autosend and TTS from settings
    autosend_enabled = snapshot.autosendEnabled;
    auto_tts_enabled = settings.getTtsEnabled();
    if (autosend_checkbox) {
        if (autosend_enabled) {
            lv_obj_add_state(autosend_checkbox, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(autosend_checkbox, LV_STATE_CHECKED);
        }
    }
    
    applyTheme(snapshot);
    updateStatusIcons();
    loadConversationHistory();
    updateLayout(snapshot.landscapeLayout);
    if (chat_input) lv_group_focus_obj(chat_input);

    // Ensure dock layer is visible
    DisplayManager::getInstance().getLauncherLayer();

    // Ensure VoiceAssistant ready
    if (snapshot.voiceAssistantEnabled && !VoiceAssistant::getInstance().isInitialized()) {
        if (!VoiceAssistant::getInstance().begin()) {
            log_e("[AiChatScreen] Failed to initialize VoiceAssistant on show");
        }
    }
}

void AiChatScreen::onHide() {
    // Stop recording if active
    if (recording) {
        stopRecording();
    }
    KeyboardManager::getInstance().hide();
}

void AiChatScreen::applyTheme(const SettingsSnapshot& snapshot) {
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t card = lv_color_hex(snapshot.cardColor);
    lv_color_t text = lv_color_hex(0xffffff);
    lv_color_t subtle = lv_color_mix(accent, text, LV_OPA_30);
    lv_color_t highlight = lv_color_mix(accent, card, LV_OPA_60);
    lv_color_t muted = lv_color_hex(0x9FB0C6);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    }
    if (header_label) {
        lv_obj_set_style_text_color(header_label, accent, 0);
    }
    if (status_chip) {
        lv_obj_set_style_bg_color(status_chip, lv_color_mix(accent, primary, LV_OPA_50), 0);
        lv_obj_set_style_text_color(status_label, text, 0);
    }

    auto style_card = [&](lv_obj_t* card_obj) {
        if (!card_obj) return;
        lv_obj_set_style_bg_color(card_obj, card, 0);
        lv_obj_set_style_bg_opa(card_obj, LV_OPA_80, 0);
        lv_obj_set_style_radius(card_obj, snapshot.borderRadius, 0);
        lv_obj_set_style_shadow_width(card_obj, 8, 0);
        lv_obj_set_style_shadow_opa(card_obj, LV_OPA_20, 0);
        lv_obj_set_style_shadow_color(card_obj, lv_color_mix(accent, lv_color_hex(0x000000), LV_OPA_40), 0);
    };

    style_card(status_card);
    style_card(chat_card);
    style_card(input_card);

    if (chat_container) {
        lv_obj_set_style_bg_color(chat_container, card, 0);
        lv_obj_set_style_radius(chat_container, snapshot.borderRadius / 2, 0);
    }

    auto style_btn = [&](lv_obj_t* btn) {
        if (!btn) return;
        lv_obj_set_style_bg_color(btn, highlight, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, text, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, snapshot.borderRadius / 2 + 6, LV_PART_MAIN);

        lv_obj_t* btn_lbl = lv_obj_get_child(btn, 0);
        if (btn_lbl) lv_obj_set_style_text_color(btn_lbl, text, LV_PART_MAIN);

        // Pressed
        lv_obj_set_style_bg_color(btn, accent, LV_PART_MAIN | LV_STATE_PRESSED);
    };
    style_btn(send_button);

    // Style bottom buttons
    style_btn(reset_buffer_button);
    style_btn(save_settings_button);

    // Style PTT button
    if (ptt_button) {
        lv_obj_set_style_bg_color(ptt_button, accent, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ptt_button, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(ptt_button, snapshot.borderRadius, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(ptt_button, 12, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(ptt_button, LV_OPA_30, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(ptt_button, lv_color_mix(accent, lv_color_hex(0x000000), LV_OPA_50), LV_PART_MAIN);

        // Text and icon colors
        lv_obj_t* ptt_icon = lv_obj_get_child(ptt_button, 0);
        if (ptt_icon) lv_obj_set_style_text_color(ptt_icon, text, LV_PART_MAIN);
        lv_obj_t* ptt_label = lv_obj_get_child(ptt_button, 1);
        if (ptt_label) lv_obj_set_style_text_color(ptt_label, text, LV_PART_MAIN);

        // Pressed state - lighter accent
        lv_color_t pressed_color = lv_color_mix(accent, text, LV_OPA_30); // Lighter
        lv_obj_set_style_bg_color(ptt_button, pressed_color, LV_PART_MAIN | LV_STATE_PRESSED);
    }

    if (chat_input) {
        lv_obj_set_style_bg_color(chat_input, lv_color_mix(card, accent, LV_OPA_20), LV_PART_MAIN);
        lv_obj_set_style_text_color(chat_input, text, LV_PART_MAIN);
        lv_obj_set_style_border_width(chat_input, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(chat_input, snapshot.borderRadius / 2, LV_PART_MAIN);
        lv_obj_set_style_text_color(chat_input, muted, LV_PART_TEXTAREA_PLACEHOLDER);
    }
    if (autosend_checkbox) {
        lv_obj_set_style_text_color(autosend_checkbox, subtle, 0);
    }
    if (memory_label) {
        lv_obj_set_style_text_color(memory_label, subtle, 0);
    }

    updateStatusIcons();
}

void AiChatScreen::updateStatusIcons() {
    if (!status_bar) return;

    const SettingsSnapshot& snapshot = SettingsManager::getInstance().getSnapshot();
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t inactive = lv_color_hex(0x606060);

    // WiFi
    bool wifi_connected = WiFi.status() == WL_CONNECTED;
    lv_obj_set_style_text_color(wifi_status_label, wifi_connected ? accent : inactive, 0);
    lv_obj_set_style_opa(wifi_status_label, wifi_connected ? LV_OPA_COVER : LV_OPA_50, 0);

    // BLE
    BleHidManager& ble = BleHidManager::getInstance();
    bool ble_active = ble.isEnabled() && ble.isInitialized();
    lv_obj_set_style_text_color(ble_status_label, ble_active ? accent : inactive, 0);
    lv_obj_set_style_opa(ble_status_label, ble_active ? LV_OPA_COVER : LV_OPA_50, 0);

    // SD
    SdCardDriver& sd = SdCardDriver::getInstance();
    bool sd_mounted = sd.isMounted();
    lv_obj_set_style_text_color(sd_status_label, sd_mounted ? accent : inactive, 0);
    lv_obj_set_style_opa(sd_status_label, sd_mounted ? LV_OPA_COVER : LV_OPA_50, 0);

    // AI status
    bool ai_ready = snapshot.voiceAssistantEnabled && VoiceAssistant::getInstance().isInitialized();
    lv_obj_set_style_text_color(ai_status_label, ai_ready ? accent : inactive, 0);
    lv_obj_set_style_opa(ai_status_label, ai_ready ? LV_OPA_COVER : LV_OPA_50, 0);
}

void AiChatScreen::loadConversationHistory() {
    lv_obj_clean(chat_container); // Clear children

    ConversationBuffer& buffer = ConversationBuffer::getInstance();
    if (!buffer.begin()) {
        log_e("[AiChatScreen] Failed to access conversation buffer");
        appendMessage("assistant", "Errore nel caricamento della conversazione.");
        lv_obj_scroll_to_y(root, LV_COORD_MAX, LV_ANIM_OFF);
        return;
    }

    auto entries = buffer.getEntries();
    int limit = buffer.getLimit();
    int size = entries.size();

    if (memory_label) {
        lv_label_set_text_fmt(memory_label, "Buffer: %d / %d", size, limit);
    }

    if (entries.empty()) {
        appendMessage("assistant", "Ciao! Dimmi cosa fare.");
        lv_obj_scroll_to_y(root, LV_COORD_MAX, LV_ANIM_OFF);
        return;
    }

    for (const auto& entry : entries) {
        String role = entry.role.c_str();
        String text = entry.text.c_str();
        String meta = "";
        if (!entry.command.empty()) {
            meta += "Comando: " + String(entry.command.c_str());
        }
        if (!entry.args.empty()) {
            // Serialize args simply
            meta += " · Argomenti: [";
            for (size_t i = 0; i < entry.args.size(); ++i) {
                if (i > 0) meta += ", ";
                meta += entry.args[i].c_str();
            }
            meta += "]";
        }
        String output = entry.output.c_str();
        appendMessage(role, text, meta, output);
    }

    lv_obj_scroll_to_y(root, LV_COORD_MAX, LV_ANIM_OFF);
}

void AiChatScreen::appendMessage(const String& role, const String& text, const String& meta, const String& output) {
    lv_obj_t* bubble = lv_obj_create(chat_container);
    lv_obj_set_size(bubble, lv_pct(85), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(bubble, 14, 0);
    lv_obj_set_style_radius(bubble, 14, 0);
    lv_obj_set_style_border_width(bubble, 1, 0);
    lv_obj_set_style_border_opa(bubble, LV_OPA_20, 0);
    lv_obj_set_style_flex_grow(bubble, 0, 0); // No grow

    const SettingsSnapshot& snapshot = SettingsManager::getInstance().getSnapshot();
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t muted = lv_color_hex(0x9FB0C6);
    lv_color_t bg_user = lv_color_mix(accent, lv_color_black(), 46); // Approx 18%
    lv_color_t bg_assistant = lv_color_hex(0x0a0a0a);

    lv_obj_t* content = lv_label_create(bubble);
    lv_label_set_text(content, text.c_str());
    lv_obj_set_style_text_color(content, lv_color_white(), 0);
    lv_label_set_long_mode(content, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(content, lv_pct(100));

    if (role == "user") {
        lv_obj_align(bubble, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(bubble, bg_user, 0);
        lv_obj_set_style_border_color(bubble, accent, 0);
    } else {
        lv_obj_align(bubble, LV_ALIGN_OUT_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(bubble, bg_assistant, 0);
        lv_obj_set_style_border_color(bubble, muted, 0);

        // Optional: Add manual speak button for assistant messages
        lv_obj_t* speak_btn = lv_btn_create(bubble);
        lv_obj_set_size(speak_btn, 30, 30);
        lv_obj_set_style_radius(speak_btn, 50, 0);  // Circle
        lv_obj_t* icon = lv_label_create(speak_btn);
        lv_label_set_text(icon, "🔊");  // Speaker emoji or icon
        lv_obj_add_event_cb(speak_btn, [](lv_event_t* e) {
            AiChatScreen* screen = AiChatScreen::instance;
            if (screen) {
                lv_obj_t* target = lv_event_get_target(e);
                lv_obj_t* bubble_parent = target->parent;
                lv_obj_t* content_label = nullptr;
                uint32_t child_count = lv_obj_get_child_cnt(bubble_parent);
                for (uint32_t i = 0; i < child_count; i++) {
                    lv_obj_t* child = lv_obj_get_child(bubble_parent, i);
                    if (lv_obj_check_type(child, &lv_label_class) && strcmp(lv_label_get_text(child), "🔊") != 0) {
                        content_label = child;
                        break;
                    }
                }
                if (content_label) {
                    const char* text_c = lv_label_get_text(content_label);
                    String msg_text(text_c);
                    if (!msg_text.isEmpty()) {
                        String escaped_text = screen->escapeLuaString(msg_text);
                        std::string lua_script = "speak_and_play(\"" + std::string(escaped_text.c_str()) + "\")";
                        CommandResult res = screen->lua_sandbox_.execute(lua_script);
                        if (res.success) {
                            screen->setStatus("Parlando...", lv_color_hex(0x7EE7C0));
                            screen->tts_playing = true;
                        } else {
                            screen->setStatus("Errore TTS", lv_color_hex(0xFF7B7B));
                        }
                    }
                }
            }
        }, LV_EVENT_CLICKED, nullptr);
        lv_obj_align(speak_btn, LV_ALIGN_OUT_RIGHT_TOP, -10, 5);
    }

    if (meta.length() > 0) {
        lv_obj_t* meta_label = lv_label_create(bubble);
        lv_label_set_text(meta_label, meta.c_str());
        lv_obj_set_style_text_color(meta_label, muted, 0);
        lv_label_set_long_mode(meta_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(meta_label, lv_pct(100));
    }

    if (output.length() > 0) {
        lv_obj_t* output_label = lv_label_create(bubble);
        lv_label_set_text(output_label, output.c_str());
        lv_obj_set_style_text_color(output_label, accent, 0);
        lv_label_set_long_mode(output_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(output_label, lv_pct(100));
    }

    lv_obj_update_layout(chat_container);
    lv_obj_scroll_to_y(root, LV_COORD_MAX, LV_ANIM_OFF); // Scroll root to bottom
}

String AiChatScreen::escapeLuaString(const String& s) {
    String escaped = s;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    escaped.replace("\t", "\\t");
    return escaped;
}

void AiChatScreen::sendChatMessage() {
    if (recording) return;

    const char* text_c = lv_textarea_get_text(chat_input);
    if (!text_c || strlen(text_c) == 0) return;

    String text(text_c);
    lv_textarea_set_text(chat_input, "");
    appendMessage("user", text);
    lv_obj_add_state(send_button, LV_STATE_DISABLED);
    setStatus("Elaborando...", lv_color_hex(0x7EE7C0));

    SettingsManager& settings = SettingsManager::getInstance();
    if (!settings.getVoiceAssistantEnabled()) {
        appendMessage("assistant", "AI non abilitato nelle impostazioni.", "error", "");
        setStatus("Errore", lv_color_hex(0xFF7B7B));
        lv_obj_clear_state(send_button, LV_STATE_DISABLED);
        return;
    }

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    if (!assistant.isInitialized()) {
        if (!assistant.begin()) {
            appendMessage("assistant", "Impossibile inizializzare l'AI.", "error", "");
            setStatus("Errore", lv_color_hex(0xFF7B7B));
            lv_obj_clear_state(send_button, LV_STATE_DISABLED);
            return;
        }
    }

    AsyncRequestManager& manager = AsyncRequestManager::getInstance();
    if (!manager.isRunning() && !manager.begin()) {
        appendMessage("assistant", "Sistema di elaborazione non disponibile.", "error", "");
        setStatus("Errore", lv_color_hex(0xFF7B7B));
        lv_obj_clear_state(send_button, LV_STATE_DISABLED);
        return;
    }

    current_request_id = "";
    if (!manager.submitRequest(text.c_str(), current_request_id)) {
        appendMessage("assistant", "Impossibile inviare la richiesta.", "error", "");
        setStatus("Errore", lv_color_hex(0xFF7B7B));
        lv_obj_clear_state(send_button, LV_STATE_DISABLED);
        return;
    }

    polling_active = true;
    if (!poll_timer) {
        poll_timer = lv_timer_create(pollRequestTimer, 3000, this);
    }

    lv_obj_clear_state(send_button, LV_STATE_DISABLED);
}

void AiChatScreen::startRecording() {
    SettingsManager& settings = SettingsManager::getInstance();
    if (!settings.getVoiceAssistantEnabled()) {
        setStatus("AI non abilitato", lv_color_hex(0xFF7B7B));
        return;
    }

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    if (!assistant.isInitialized()) {
        if (!assistant.begin()) {
            setStatus("Impossibile inizializzare l'AI", lv_color_hex(0xFF7B7B));
            return;
        }
    }

    if (recording) return; // Already recording

    // Start
    recording = true;
    lv_obj_add_state(ptt_button, LV_STATE_PRESSED); // Visual feedback
    lv_obj_set_style_bg_color(ptt_button, lv_color_hex(0xFF6B6B), LV_PART_MAIN); // Red
    setStatus("Registrazione in corso...", lv_color_hex(0xFF6B6B));
    assistant.startRecording();
    lv_obj_add_state(send_button, LV_STATE_DISABLED);
    updateStatusIcons();
}

void AiChatScreen::stopRecording() {
    if (!recording) return;

    // Stop
    recording = false;
    lv_obj_clear_state(ptt_button, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(ptt_button, lv_color_hex(instance ? lv_color_hex(SettingsManager::getInstance().getSnapshot().accentColor).full : 0x00FF00), LV_PART_MAIN); // Reset to accent
    lv_obj_clear_state(send_button, LV_STATE_DISABLED);

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    assistant.stopRecordingAndProcess();

    // Get transcription first (STT only)
    std::string transcription;
    const uint32_t stt_timeout_ms = 30000; // 30s for STT
    if (assistant.getLastTranscription(transcription, stt_timeout_ms)) {
        setStatus("Trascrizione completata", lv_color_hex(0x7EE7C0));

        if (autosend_enabled) {
            // Auto-send to LLM
            appendMessage("user", transcription.c_str());
            setStatus("Elaborando con AI...", lv_color_hex(0x7EE7C0));
            lv_obj_add_state(send_button, LV_STATE_DISABLED);

            // Send to LLM
            std::string req_id;
            if (assistant.sendTextMessage(transcription)) {
                // Wait for LLM response
                const uint32_t llm_timeout_ms = 120000; // 2 min for LLM
                VoiceAssistant::VoiceCommand response;
                if (assistant.getLastResponse(response, llm_timeout_ms)) {
                    String r_text = String(response.text.c_str());
                    String meta = response.command.empty() ? "" : "Comando: " + String(response.command.c_str());
                    if (r_text.length() > 0) {
                        appendMessage("assistant", r_text, meta, response.output.c_str());
                    }
                    String status_msg = "Risposta AI generata";
                    lv_color_t status_color = lv_color_hex(0x70FFBA);
                    setStatus(status_msg, status_color);
                } else {
                    appendMessage("assistant", "Timeout nell'elaborazione AI.", "error", "");
                    setStatus("Timeout AI", lv_color_hex(0xFF7B7B));
                }
            } else {
                appendMessage("assistant", "Errore nell'invio al AI.", "error", "");
                setStatus("Errore invio", lv_color_hex(0xFF7B7B));
            }
            lv_obj_clear_state(send_button, LV_STATE_DISABLED);
        } else {
            // Just set transcription in input
            if (!transcription.empty()) {
                lv_textarea_set_text(chat_input, transcription.c_str());
                if (chat_input) lv_group_focus_obj(chat_input);
            }
            String status_msg = "Trascrizione pronta per invio manuale";
            lv_color_t status_color = lv_color_hex(0x70FFBA);
            setStatus(status_msg, status_color);
        }
        loadConversationHistory(); // Refresh UI
    } else {
        appendMessage("assistant", "Timeout o errore nella trascrizione audio.", "error", "");
        setStatus("Errore STT", lv_color_hex(0xFF7B7B));
    }
    updateStatusIcons();
}

bool AiChatScreen::handleTranscription(const String& transcription) {
    if (polling_active) {
        stopPolling();
    }
    if (transcription.length() == 0) return false;

    if (autosend_enabled) {
        lv_textarea_set_text(chat_input, transcription.c_str());
        sendChatMessage(); // Auto send
    } else {
        lv_textarea_set_text(chat_input, transcription.c_str());
        if (chat_input) lv_group_focus_obj(chat_input);
    }
    return true;
}

void AiChatScreen::stopPolling() {
    polling_active = false;
    if (poll_timer) {
        lv_timer_del(poll_timer);
        poll_timer = nullptr;
    }
    setStatus("Pronto", lv_color_hex(0x70FFBA));
    current_request_id = "";
}

void AiChatScreen::setStatus(const String& text, lv_color_t color) {
    if (status_label) {
        lv_label_set_text(status_label, text.c_str());
        lv_obj_set_style_text_color(status_label, color, 0);
    }
}

void AiChatScreen::updateLayout(bool landscape) {
    // No change needed for content, as root height is fixed and dock is global
    if (!content_container) return;

    lv_obj_set_flex_flow(content_container, landscape ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);

    auto set_card_width = [&](lv_obj_t* card) {
        if (!card) return;
        lv_obj_set_width(card, landscape ? lv_pct(48) : lv_pct(100));
    };
    set_card_width(status_card);
    set_card_width(chat_card);
    set_card_width(input_card);

    // In landscape, dock might need horizontal adjustment, but since it's global, root width adjustment suffices
}

void AiChatScreen::statusUpdateTimer(lv_timer_t* timer) {
    if (instance) {
        instance->updateStatusIcons();
    }
}

void AiChatScreen::pollRequestTimer(lv_timer_t* timer) {
    if (instance && instance->polling_active && !instance->current_request_id.empty()) {
        AsyncRequestManager& manager = AsyncRequestManager::getInstance();
        AsyncRequestManager::RequestResult result;
        if (manager.getRequestStatus(instance->current_request_id, result)) {
            if (result.status == AsyncRequestManager::RequestStatus::COMPLETED || 
                result.status == AsyncRequestManager::RequestStatus::FAILED ||
                result.status == AsyncRequestManager::RequestStatus::TIMEOUT) {
                instance->stopPolling();
                if (result.status == AsyncRequestManager::RequestStatus::COMPLETED) {
                    VoiceAssistant::VoiceCommand resp = result.response;
                    String meta = resp.command.empty() ? "" : "Comando: " + String(resp.command.c_str());
                    instance->appendMessage("assistant", resp.text.c_str(), meta, resp.output.c_str());

                    // Automatic TTS trigger
                    if (!resp.text.empty() && instance->auto_tts_enabled) {
                        String resp_str(resp.text.c_str());
                        String escaped_text = instance->escapeLuaString(resp_str);
                        std::string lua_script = "speak_and_play(\"" + std::string(escaped_text.c_str()) + "\")";
                        CommandResult res = instance->lua_sandbox_.execute(lua_script);
                        if (res.success) {
                            instance->setStatus("Parlando risposta...", lv_color_hex(0x7EE7C0));
                            instance->tts_playing = true;
                            // Start TTS status timer if not already
                            if (!instance->tts_status_timer) {
                                instance->tts_status_timer = lv_timer_create([](lv_timer_t* t) {
                                    AiChatScreen* screen = static_cast<AiChatScreen*>(t->user_data);
                                    if (screen && screen->tts_playing) {
                                        // Call Lua to check radio status
                                        CommandResult status_res = screen->lua_sandbox_.execute("local _, status = radio.status(); return status");
                                        if (status_res.success) {
                                            if (status_res.message.find("ENDED") != std::string::npos || status_res.message.find("STOPPED") != std::string::npos) {
                                                screen->tts_playing = false;
                                                screen->setStatus("Risposta completata", lv_color_hex(0x70FFBA));
                                                if (screen->tts_status_timer) {
                                                    lv_timer_del(screen->tts_status_timer);
                                                    screen->tts_status_timer = nullptr;
                                                }
                                            }
                                        }
                                    }
                                }, 1000, instance);
                            }
                        } else {
                            instance->setStatus("Errore TTS", lv_color_hex(0xFF7B7B));
                        }
                    } else {
                        instance->setStatus("Risposta ricevuta", lv_color_hex(0x70FFBA));
                    }
                } else {
                    String err_msg = "Errore nell'elaborazione.";
                    if (!result.error_message.empty()) {
                        err_msg = String("Errore: ") + result.error_message.c_str();
                    }
                    instance->appendMessage("assistant", err_msg, "error", "");
                    instance->setStatus("Errore", lv_color_hex(0xFF7B7B));
                }
                instance->loadConversationHistory(); // Refresh
                return;
            }
        } else {
            // Request not found, stop polling
            instance->stopPolling();
            instance->appendMessage("assistant", "Richiesta non trovata.", "error", "");
            instance->setStatus("Errore", lv_color_hex(0xFF7B7B));
        }
    }
}

void AiChatScreen::pttPressedEvent(lv_event_t* e) {
    if (instance) {
        instance->startRecording();
    }
}

void AiChatScreen::pttReleasedEvent(lv_event_t* e) {
    if (instance) {
        instance->stopRecording();
    }
}

void AiChatScreen::sendButtonEvent(lv_event_t* e) {
    if (instance) {
        instance->sendChatMessage();
    }
}

void AiChatScreen::inputEvent(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (instance && code == LV_EVENT_KEY) {
        lv_indev_t * indev = lv_indev_get_act();
        if(indev) {
            uint32_t key = lv_indev_get_key(indev);
            if(key == LV_KEY_ENTER) {
                instance->sendChatMessage();
            }
        }
    } else if (code == LV_EVENT_FOCUSED) {
        KeyboardManager::getInstance().showForTextArea(lv_event_get_target(e));
    } else if (code == LV_EVENT_DEFOCUSED) {
        KeyboardManager::getInstance().hide();
    }
}

void AiChatScreen::autosendEvent(lv_event_t* e) {
    if (instance) {
        bool enabled = lv_obj_has_state(e->target, LV_STATE_CHECKED);
        instance->autosend_enabled = enabled;
        SettingsManager::getInstance().setAutosendEnabled(enabled);
        instance->setStatus("Autosend aggiornato", lv_color_hex(0x70FFBA));
    }
}

void AiChatScreen::resetBufferEvent(lv_event_t* e) {
    if (instance) {
        ConversationBuffer& buffer = ConversationBuffer::getInstance();
        if (buffer.begin()) {
            buffer.clear();
            instance->loadConversationHistory();
            instance->setStatus("Buffer resettato", lv_color_hex(0x70FFBA));
        } else {
            instance->setStatus("Errore reset buffer", lv_color_hex(0xFF7B7B));
        }
    }
}

void AiChatScreen::saveSettingsEvent(lv_event_t* e) {
    if (instance) {
        SettingsManager& settings = SettingsManager::getInstance();
        settings.setAutosendEnabled(instance->autosend_enabled);
        instance->setStatus("Impostazioni salvate", lv_color_hex(0x70FFBA));
    }
}

void AiChatScreen::staticInit() {
    // Any static initialization if needed
}

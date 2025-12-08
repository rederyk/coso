#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"
#include "core/microphone_manager.h"
#include "core/voice_assistant.h"
#include "ui/ui_symbols.h"
#include <lvgl.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class AiChatScreen : public Screen {
public:
    ~AiChatScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

    static AiChatScreen* instance;

    static void pttPressedEvent(lv_event_t* e);
    static void pttReleasedEvent(lv_event_t* e);

private:
    void applyTheme(const SettingsSnapshot& snapshot);
    void updateStatusIcons();
    void loadConversationHistory();
    void appendMessage(const String& role, const String& text, const String& meta = "", const String& output = "");
    void sendChatMessage();
    void toggleRecording();
    bool handleTranscription(const String& transcription);
    void setStatus(const String& text, lv_color_t color);
    void updateLayout(bool landscape);
    void stopPolling();
    static void statusUpdateTimer(lv_timer_t* timer);
    static void pollRequestTimer(lv_timer_t* timer);
    static void sendButtonEvent(lv_event_t* e);
    static void micButtonEvent(lv_event_t* e);
    static void inputEvent(lv_event_t* e);
    static void autosendEvent(lv_event_t* e);

    String escapeLuaString(const String& s);

    lv_obj_t* root = nullptr;
    lv_obj_t* header_container = nullptr;
    lv_obj_t* status_bar = nullptr;
    lv_obj_t* status_chip = nullptr;
    lv_obj_t* wifi_status_label = nullptr;
    lv_obj_t* ble_status_label = nullptr;
    lv_obj_t* sd_status_label = nullptr;
    lv_obj_t* ai_status_label = nullptr;
    lv_obj_t* header_label = nullptr;
    lv_obj_t* content_container = nullptr;
    lv_obj_t* status_card = nullptr;
    lv_obj_t* chat_card = nullptr;
    lv_obj_t* chat_container = nullptr;  // Messages holder (expands)
    lv_obj_t* input_card = nullptr;
    lv_obj_t* chat_input = nullptr;
    lv_obj_t* send_button = nullptr;
    lv_obj_t* mic_button = nullptr;
    lv_obj_t* autosend_checkbox = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* memory_label = nullptr;
    lv_obj_t* ptt_button = nullptr;
    lv_obj_t* ptt_container = nullptr;
    lv_obj_t* actions_bottom_container = nullptr;
    lv_obj_t* reset_buffer_button = nullptr;
    lv_obj_t* save_settings_button = nullptr;

    lv_timer_t* status_timer = nullptr;
    lv_timer_t* poll_timer = nullptr;
    lv_timer_t* tts_status_timer = nullptr;
    uint32_t settings_listener_id = 0;
    bool recording = false;
    bool autosend_enabled = true;
    bool auto_tts_enabled = true;
    bool updating_from_manager = false;
    bool polling_active = false;
    bool tts_playing = false;
    std::string current_request_id;

    VoiceAssistant::LuaSandbox lua_sandbox_;

    void staticInit();

    void startRecording();
    void stopRecording();
    static void resetBufferEvent(lv_event_t* e);
    static void saveSettingsEvent(lv_event_t* e);
};

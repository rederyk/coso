#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include "core/screen.h"
#include "core/settings_manager.h"
#include "core/microphone_manager.h"

class MicrophoneTestScreen : public Screen {
public:
    static constexpr lv_coord_t CARD_HEIGHT_PX = 80;

    ~MicrophoneTestScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

protected:
    void applySnapshot(const SettingsSnapshot& snapshot);
    void applyThemeStyles(const SettingsSnapshot& snapshot);
    void refreshAudioFilesList();
    void updateMicLevelIndicator(uint16_t level);
    void requestStopRecording();
    static void recordingTask(void* param);

    static void handleRecordStartButton(lv_event_t* e);
    static void handleRecordStopButton(lv_event_t* e);
    static void handlePlaybackButton(lv_event_t* e);
    static void handleAudioFileButton(lv_event_t* e);

    lv_obj_t* title_label = nullptr;
    lv_obj_t* record_card = nullptr;
    lv_obj_t* playback_card = nullptr;
    lv_obj_t* files_card = nullptr;
    lv_obj_t* record_start_button = nullptr;
    lv_obj_t* record_stop_button = nullptr;
    lv_obj_t* mic_level_arc = nullptr;
    lv_obj_t* mic_level_label = nullptr;
    lv_obj_t* playback_button = nullptr;
    lv_obj_t* record_status_label = nullptr;
    lv_obj_t* playback_status_label = nullptr;
    lv_obj_t* files_container = nullptr;

    uint32_t settings_listener_id = 0;
    bool updating_from_manager = false;
    bool is_recording = false;
    bool is_playing = false;
    TaskHandle_t recording_task_handle = nullptr;
    std::string current_playback_file;
    std::atomic<bool> stop_recording_requested{false};
    std::atomic<bool> screen_valid{true};  // Flag to indicate if screen is still valid
    MicrophoneManager::RecordingHandle recording_handle = nullptr;
};

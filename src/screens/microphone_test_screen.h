#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "core/screen.h"
#include "core/settings_manager.h"

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
    static void recordingTask(void* param);

    static void handleRecordButton(lv_event_t* e);
    static void handlePlaybackButton(lv_event_t* e);
    static void handleAudioFileButton(lv_event_t* e);

    lv_obj_t* title_label = nullptr;
    lv_obj_t* record_card = nullptr;
    lv_obj_t* playback_card = nullptr;
    lv_obj_t* files_card = nullptr;
    lv_obj_t* record_button = nullptr;
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
};

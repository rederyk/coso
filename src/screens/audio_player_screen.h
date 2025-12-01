#pragma once

#include "core/screen.h"
#include <lvgl.h>
#include <string>

struct Metadata;

/**
 * Audio Player Screen
 * Displays file playback with controls and metadata
 */
class AudioPlayerScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

    enum class StorageSource {
        SdCard,
        LittleFS,
    };

private:
    void refreshFileList();
    void updatePlaybackInfo();
    void updateProgressBar();
    void formatTime(char* buffer, size_t size, uint32_t ms);
    void navigateToDirectory(StorageSource source, const std::string& path);

    static void onFileSelected(lv_event_t* event);
    static void onPlayPauseClicked(lv_event_t* event);
    static void onStopClicked(lv_event_t* event);
    static void onVolumeChanged(lv_event_t* event);
    static void onUpdateTimer(lv_timer_t* timer);
    static void onProgressCallback(uint32_t pos_ms, uint32_t dur_ms);
    static void onMetadataCallback(const Metadata& meta);

    // UI Elements
    lv_obj_t* file_list = nullptr;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* artist_label = nullptr;
    lv_obj_t* progress_bar = nullptr;
    lv_obj_t* time_label = nullptr;
    lv_obj_t* play_pause_btn = nullptr;
    lv_obj_t* play_pause_label = nullptr;
    lv_obj_t* volume_slider = nullptr;
    lv_timer_t* update_timer = nullptr;

    std::string current_path_;
    std::string sd_current_path_ = "/";
    std::string littlefs_current_path_ = "/";
};

#pragma once

#include "core/screen.h"
#include <lvgl.h>

struct Metadata;

/**
 * Web Radio Screen
 * Internet radio streaming with timeshift capabilities
 */
class WebRadioScreen : public Screen {
public:
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void refreshStationList();
    void updatePlaybackInfo();
    void showAddStationDialog();
    void updateProgress(uint32_t pos_ms, uint32_t dur_ms);
    void refreshProgressLabels(uint32_t display_pos_ms, uint32_t dur_ms);
    void seekToSliderValue(int slider_value);
    uint32_t sliderValueToMs(int slider_value, uint32_t duration_ms) const;
    static void formatTime(char* buffer, size_t size, uint32_t ms);

    static void onStationSelected(lv_event_t* event);
    static void onPlayPauseClicked(lv_event_t* event);
    static void onStopClicked(lv_event_t* event);
    static void onAddStationClicked(lv_event_t* event);
    static void onVolumeChanged(lv_event_t* event);
    static void onUpdateTimer(lv_timer_t* timer);
    static void onMetadataCallback(const Metadata& meta);
    static void onProgressSliderEvent(lv_event_t* event);

    // UI Elements
    lv_obj_t* station_list = nullptr;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* station_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* progress_slider = nullptr;
    lv_obj_t* progress_time_label = nullptr;
    lv_obj_t* shift_info_label = nullptr;
    lv_obj_t* play_pause_btn = nullptr;
    lv_obj_t* play_pause_label = nullptr;
    lv_obj_t* volume_slider = nullptr;
    lv_obj_t* add_station_btn = nullptr;
    lv_timer_t* update_timer = nullptr;

    size_t current_station_index_;
    bool progress_dragging_ = false;
    uint32_t last_position_ms_ = 0;
    uint32_t last_duration_ms_ = 0;

    static constexpr int PROGRESS_SLIDER_MAX = 1000;
};

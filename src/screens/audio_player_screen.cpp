#include "screens/audio_player_screen.h"
#include "core/audio_manager.h"
#include "core/settings_manager.h"
#include "drivers/sd_card_driver.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/color_utils.h"
#include <algorithm>

namespace {
    constexpr uint32_t UPDATE_INTERVAL_MS = 500;

    lv_obj_t* create_card(lv_obj_t* parent, lv_color_t bg_color) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(card, bg_color, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_pad_all(card, 12, 0);
        return card;
    }

    void style_list_button(lv_obj_t* btn, lv_color_t bg_color) {
        if (!btn) return;
        lv_obj_set_style_bg_color(btn, bg_color, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
        ColorUtils::applyAutoButtonTextColor(btn);
    }
}

void AudioPlayerScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    const auto& settings = SettingsManager::getInstance().getSnapshot();
    lv_color_t primary_color = lv_color_hex(settings.primaryColor);
    lv_color_t accent_color = lv_color_hex(settings.accentColor);
    lv_color_t card_color = lv_color_hex(settings.cardColor);
    lv_color_t dock_color = lv_color_hex(settings.dockColor);
    lv_color_t primary_text = ColorUtils::invertColor(primary_color);
    lv_color_t card_text = ColorUtils::invertColor(card_color);
    lv_color_t muted_card_text = ColorUtils::getMutedTextColor(card_color);
    lv_color_t dock_text = ColorUtils::invertColor(dock_color);
    lv_color_t list_card_color = lv_color_mix(card_color, dock_color, LV_OPA_60);

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(root, primary_color, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_set_style_pad_row(root, 8, 0);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);

    // Header
    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text(header, LV_SYMBOL_AUDIO " Music Player");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(header, accent_color, 0);

    // Now Playing Card
    lv_obj_t* now_playing_card = create_card(root, card_color);
    lv_obj_set_style_pad_all(now_playing_card, 16, 0);

    title_label = lv_label_create(now_playing_card);
    lv_label_set_text(title_label, "No track playing");
    lv_obj_set_width(title_label, lv_pct(100));
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_label, card_text, 0);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    artist_label = lv_label_create(now_playing_card);
    lv_label_set_text(artist_label, "");
    lv_obj_set_width(artist_label, lv_pct(100));
    lv_obj_set_style_text_font(artist_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(artist_label, muted_card_text, 0);

    // Progress bar
    progress_bar = lv_bar_create(now_playing_card);
    lv_obj_set_width(progress_bar, lv_pct(100));
    lv_obj_set_height(progress_bar, 8);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_obj_set_style_bg_color(progress_bar, lv_color_mix(card_color, primary_color, LV_OPA_30), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_bar, accent_color, LV_PART_INDICATOR);

    // Time label
    time_label = lv_label_create(now_playing_card);
    lv_label_set_text(time_label, "0:00 / 0:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_label, muted_card_text, 0);

    // Controls Card
    lv_obj_t* controls_card = create_card(root, lv_color_mix(dock_color, primary_color, LV_OPA_40));
    lv_obj_set_layout(controls_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(controls_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(controls_card, 8, 0);

    // Play/Pause button
    play_pause_btn = lv_btn_create(controls_card);
    lv_obj_set_size(play_pause_btn, 60, 60);
    lv_obj_add_event_cb(play_pause_btn, onPlayPauseClicked, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(play_pause_btn, accent_color, 0);
    lv_obj_set_style_radius(play_pause_btn, 30, 0);

    play_pause_label = lv_label_create(play_pause_btn);
    lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(play_pause_label, &lv_font_montserrat_24, 0);
    lv_obj_center(play_pause_label);
    ColorUtils::applyAutoButtonTextColor(play_pause_btn);

    // Stop button
    lv_obj_t* stop_btn = lv_btn_create(controls_card);
    lv_obj_set_size(stop_btn, 50, 50);
    lv_obj_add_event_cb(stop_btn, onStopClicked, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(stop_btn, lv_color_mix(dock_color, card_color, LV_OPA_40), 0);
    lv_obj_set_style_radius(stop_btn, 25, 0);

    lv_obj_t* stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(stop_label, &lv_font_montserrat_20, 0);
    lv_obj_center(stop_label);
    ColorUtils::applyAutoButtonTextColor(stop_btn);

    // Volume control
    lv_obj_t* volume_label = lv_label_create(controls_card);
    lv_label_set_text(volume_label, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_color(volume_label, dock_text, 0);

    volume_slider = lv_slider_create(controls_card);
    lv_obj_set_width(volume_slider, 100);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, AudioManager::getInstance().getVolume(), LV_ANIM_OFF);
    lv_obj_add_event_cb(volume_slider, onVolumeChanged, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_set_style_bg_color(volume_slider, lv_color_mix(dock_color, card_color, LV_OPA_40), LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider, accent_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider, accent_color, LV_PART_KNOB);
    lv_obj_set_style_border_width(volume_slider, 0, LV_PART_KNOB);

    // File List Card
    lv_obj_t* list_card = create_card(root, list_card_color);
    lv_obj_set_flex_grow(list_card, 1);
    lv_obj_set_layout(list_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list_card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* list_title = lv_label_create(list_card);
    lv_label_set_text(list_title, LV_SYMBOL_SD_CARD " Music Files");
    lv_obj_set_style_text_font(list_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(list_title, ColorUtils::invertColor(list_card_color), 0);

    file_list = lv_list_create(list_card);
    lv_obj_set_size(file_list, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(file_list, 1);
    lv_obj_set_style_bg_color(file_list, lv_color_mix(list_card_color, primary_color, LV_OPA_20), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(file_list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(file_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(file_list, 8, LV_PART_MAIN);

    refreshFileList();
}

void AudioPlayerScreen::onShow() {
    auto& audio = AudioManager::getInstance();
    audio.setProgressCallback(onProgressCallback);
    audio.setMetadataCallback(onMetadataCallback);

    // Start update timer
    if (!update_timer) {
        update_timer = lv_timer_create(onUpdateTimer, UPDATE_INTERVAL_MS, this);
    }

    updatePlaybackInfo();
}

void AudioPlayerScreen::onHide() {
    auto& audio = AudioManager::getInstance();
    audio.setProgressCallback(nullptr);
    audio.setMetadataCallback(nullptr);

    // Stop update timer
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = nullptr;
    }
}

void AudioPlayerScreen::refreshFileList() {
    if (!file_list) return;

    const auto& settings = SettingsManager::getInstance().getSnapshot();
    lv_color_t list_item_color = lv_color_mix(lv_color_hex(settings.cardColor),
                                              lv_color_hex(settings.dockColor),
                                              LV_OPA_50);

    lv_obj_clean(file_list);

    auto& sd = SdCardDriver::getInstance();
    if (!sd.isMounted()) {
        lv_obj_t* btn = lv_list_add_btn(file_list, LV_SYMBOL_WARNING, "SD card not mounted");
        style_list_button(btn, list_item_color);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        return;
    }

    auto entries = sd.listDirectory("/", 100);

    for (const auto& entry : entries) {
        if (entry.isDirectory) continue;

        // Filter audio files
        const char* name = entry.name.c_str();
        const char* ext = strrchr(name, '.');
        if (!ext) continue;

        bool is_audio = (strcasecmp(ext, ".mp3") == 0 ||
                        strcasecmp(ext, ".wav") == 0 ||
                        strcasecmp(ext, ".flac") == 0 ||
                        strcasecmp(ext, ".aac") == 0);

        if (!is_audio) continue;

        lv_obj_t* btn = lv_list_add_btn(file_list, LV_SYMBOL_AUDIO, name);
        style_list_button(btn, list_item_color);
        lv_obj_add_event_cb(btn, onFileSelected, LV_EVENT_CLICKED, this);
    }

    if (lv_obj_get_child_cnt(file_list) == 0) {
        lv_obj_t* btn = lv_list_add_btn(file_list, LV_SYMBOL_FILE, "No audio files found");
        style_list_button(btn, list_item_color);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }
}

void AudioPlayerScreen::updatePlaybackInfo() {
    auto& audio = AudioManager::getInstance();
    const Metadata& meta = audio.getMetadata();

    if (audio.isPlaying() || audio.getState() == PlayerState::PAUSED) {
        if (meta.title[0] != '\0') {
            lv_label_set_text(title_label, meta.title.c_str());
        } else if (!current_path_.empty()) {
            const char* filename = strrchr(current_path_.c_str(), '/');
            lv_label_set_text(title_label, filename ? filename + 1 : current_path_.c_str());
        }

        if (meta.artist[0] != '\0') {
            lv_label_set_text(artist_label, meta.artist.c_str());
        } else {
            lv_label_set_text(artist_label, "Unknown Artist");
        }

        // Update play/pause button
        if (audio.getState() == PlayerState::PAUSED) {
            lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
        } else {
            lv_label_set_text(play_pause_label, LV_SYMBOL_PAUSE);
        }
    } else {
        lv_label_set_text(title_label, "No track playing");
        lv_label_set_text(artist_label, "");
        lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
    }
}

void AudioPlayerScreen::updateProgressBar() {
    auto& audio = AudioManager::getInstance();

    uint32_t pos_ms = audio.getCurrentPositionMs();
    uint32_t dur_ms = audio.getTotalDurationMs();

    if (dur_ms > 0) {
        uint32_t progress_pct = (pos_ms * 100) / dur_ms;
        lv_bar_set_value(progress_bar, progress_pct, LV_ANIM_OFF);
    } else {
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    }

    // Update time label
    char time_buf[32];
    formatTime(time_buf, sizeof(time_buf), pos_ms);
    strcat(time_buf, " / ");
    char dur_buf[16];
    formatTime(dur_buf, sizeof(dur_buf), dur_ms);
    strcat(time_buf, dur_buf);

    lv_label_set_text(time_label, time_buf);
}

void AudioPlayerScreen::formatTime(char* buffer, size_t size, uint32_t ms) {
    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    seconds = seconds % 60;

    snprintf(buffer, size, "%u:%02u", minutes, seconds);
}

// Event callbacks
void AudioPlayerScreen::onFileSelected(lv_event_t* event) {
    AudioPlayerScreen* screen = static_cast<AudioPlayerScreen*>(lv_event_get_user_data(event));
    lv_obj_t* btn = lv_event_get_target(event);
    lv_obj_t* label = lv_obj_get_child(btn, 1);  // Second child is the text label
    const char* filename = lv_label_get_text(label);

    std::string path = "/sdcard/";
    path += filename;

    screen->current_path_ = path;
    AudioManager::getInstance().playFile(path.c_str());
    screen->updatePlaybackInfo();

    Logger::getInstance().infof("[AudioPlayer] Selected file: %s", filename);
}

void AudioPlayerScreen::onPlayPauseClicked(lv_event_t* event) {
    AudioPlayerScreen* screen = static_cast<AudioPlayerScreen*>(lv_event_get_user_data(event));
    AudioManager::getInstance().togglePause();
    screen->updatePlaybackInfo();
}

void AudioPlayerScreen::onStopClicked(lv_event_t* event) {
    AudioPlayerScreen* screen = static_cast<AudioPlayerScreen*>(lv_event_get_user_data(event));
    AudioManager::getInstance().stop();
    screen->current_path_.clear();
    screen->updatePlaybackInfo();
    screen->updateProgressBar();
}

void AudioPlayerScreen::onVolumeChanged(lv_event_t* event) {
    lv_obj_t* slider = lv_event_get_target(event);
    int volume = lv_slider_get_value(slider);
    AudioManager::getInstance().setVolume(volume);
}

void AudioPlayerScreen::onUpdateTimer(lv_timer_t* timer) {
    AudioPlayerScreen* screen = static_cast<AudioPlayerScreen*>(timer->user_data);
    screen->updateProgressBar();
}

void AudioPlayerScreen::onProgressCallback(uint32_t pos_ms, uint32_t dur_ms) {
    // Progress updates handled by timer
}

void AudioPlayerScreen::onMetadataCallback(const Metadata& meta) {
    // Metadata will be updated on next timer tick
}

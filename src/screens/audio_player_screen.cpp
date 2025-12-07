#include "screens/audio_player_screen.h"
#include "core/audio_manager.h"
#include "core/settings_manager.h"
#include "drivers/sd_card_driver.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/color_utils.h"
#include <algorithm>
#include <LittleFS.h>
#include <cstring>
#include <strings.h>
#include <utility>

namespace {
    using StorageSource = AudioPlayerScreen::StorageSource;
    const char* storageSourceToString(StorageSource source) {
        switch (source) {
        case StorageSource::SdCard:
            return "SD";
        case StorageSource::LittleFS:
            return "LittleFS";
        }
        return "Unknown";
    }

    constexpr uint32_t UPDATE_INTERVAL_MS = 500;
    constexpr lv_coord_t DEFAULT_CARD_MIN_HEIGHT = 150;
    constexpr lv_coord_t CONTROLS_CARD_MIN_HEIGHT = 110;
    constexpr lv_coord_t LIST_CARD_MIN_HEIGHT = 220;
    constexpr size_t MAX_STORAGE_ENTRIES = 128;

    struct FileListItemData {
        StorageSource source;
        std::string path;
        bool isDirectory = false;

        FileListItemData(StorageSource source_,
                         std::string path_,
                         bool isDirectory_)
            : source(source_), path(std::move(path_)), isDirectory(isDirectory_) {}
    };

    std::string normalizePath(const std::string& raw_path) {
        if (raw_path.empty()) {
            return "/";
        }
        std::string path = raw_path;
        if (path.front() != '/') {
            path.insert(path.begin(), '/');
        }
        while (path.size() > 1 && path.back() == '/') {
            path.pop_back();
        }
        if (path.empty()) {
            path = "/";
        }

    constexpr const char* SD_PREFIX = "/sd";
    if (path.rfind(SD_PREFIX, 0) == 0) {
        path.erase(0, strlen(SD_PREFIX));
        if (path.empty()) {
            path = "/";
        }
    }
    return path;
}

    std::string combinePath(const std::string& base, const std::string& child) {
        std::string candidate;
        if (child.empty()) {
            candidate = base;
        } else if (child.front() == '/') {
            candidate = child;
        } else if (base.empty() || base == "/") {
            candidate = "/" + child;
        } else {
            candidate = base + "/" + child;
        }
        return normalizePath(candidate);
    }

    std::string displayName(const std::string& path) {
        if (path.empty()) {
            return {};
        }
        size_t slash = path.find_last_of('/');
        if (slash == std::string::npos || slash + 1 >= path.size()) {
            return path;
        }
        return path.substr(slash + 1);
    }

    std::string parentPath(const std::string& path) {
        std::string normalized = normalizePath(path);
        if (normalized == "/") {
            return "/";
        }
        size_t slash = normalized.find_last_of('/');
        if (slash == std::string::npos || slash == 0) {
            return "/";
        }
        return normalized.substr(0, slash);
    }

    bool hasAudioExtension(const std::string& filename) {
        const char* ext = strrchr(filename.c_str(), '.');
        if (!ext) {
            return false;
        }
        return strcasecmp(ext, ".mp3") == 0 ||
               strcasecmp(ext, ".wav") == 0 ||
               strcasecmp(ext, ".flac") == 0 ||
               strcasecmp(ext, ".aac") == 0;
    }

    lv_obj_t* create_card(lv_obj_t* parent, lv_color_t bg_color, lv_coord_t min_height = DEFAULT_CARD_MIN_HEIGHT) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(card, min_height, 0);
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

    void cleanup_file_button_user_data(lv_event_t* e) {
        if (!e || lv_event_get_code(e) != LV_EVENT_DELETE) return;
        lv_obj_t* target = lv_event_get_target(e);
        if (!target) return;
        auto* data_ptr = static_cast<FileListItemData*>(lv_obj_get_user_data(target));
        delete data_ptr;
        lv_obj_set_user_data(target, nullptr);
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
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(root, primary_color, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_set_style_pad_row(root, 8, 0);

    // Header
    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text(header, LV_SYMBOL_AUDIO " Music Player");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(header, accent_color, 0);

    // Now Playing Card
    lv_obj_t* now_playing_card = create_card(root, card_color, DEFAULT_CARD_MIN_HEIGHT);
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
    lv_obj_t* controls_card = create_card(root, lv_color_mix(dock_color, primary_color, LV_OPA_40), CONTROLS_CARD_MIN_HEIGHT);
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
    lv_obj_t* list_card = create_card(root, list_card_color, LIST_CARD_MIN_HEIGHT);
    lv_obj_set_layout(list_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list_card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* list_title = lv_label_create(list_card);
    lv_label_set_text(list_title, LV_SYMBOL_SD_CARD " Music Files");
    lv_obj_set_style_text_font(list_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(list_title, ColorUtils::invertColor(list_card_color), 0);

    file_list = lv_obj_create(list_card);
    lv_obj_remove_style_all(file_list);
    lv_obj_set_width(file_list, lv_pct(100));
    lv_obj_set_height(file_list, LV_SIZE_CONTENT);
    lv_obj_set_layout(file_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(file_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(file_list, 10, 0);
    lv_obj_clear_flag(file_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(file_list, lv_color_mix(list_card_color, primary_color, LV_OPA_20), 0);
    lv_obj_set_style_bg_opa(file_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(file_list, 8, 0);

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

    std::string current_sd_path = normalizePath(sd_current_path_);
    std::string current_littlefs_path = normalizePath(littlefs_current_path_);
    sd_current_path_ = current_sd_path;
    littlefs_current_path_ = current_littlefs_path;

    lv_obj_clean(file_list);

    auto add_section_label = [&](const char* text, lv_color_t color) {
        lv_obj_t* label = lv_label_create(file_list);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_color(label, color, 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_pad_bottom(label, 4, 0);
    };

    auto add_path_label = [&](const std::string& path) {
        lv_obj_t* label = lv_label_create(file_list);
        lv_label_set_text(label, path.c_str());
        lv_obj_set_style_text_color(label, ColorUtils::getMutedTextColor(list_item_color), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_pad_bottom(label, 4, 0);
    };

    auto add_message = [&](const char* text) {
        lv_obj_t* placeholder = lv_label_create(file_list);
        lv_label_set_text(placeholder, text);
        lv_obj_set_style_text_color(placeholder, ColorUtils::invertColor(list_item_color), 0);
    };

    auto add_list_button = [&](const std::string& label_text,
                               const char* icon,
                               StorageSource source,
                               const std::string& target_path,
                               bool is_directory) {
        lv_obj_t* btn = lv_btn_create(file_list);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, 50);
        lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(btn, 12, 0);
        lv_obj_set_style_pad_right(btn, 12, 0);
        lv_obj_set_style_pad_column(btn, 10, 0);
        style_list_button(btn, list_item_color);

        lv_obj_t* icon_label = lv_label_create(btn);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_color(icon_label, ColorUtils::invertColor(list_item_color), 0);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label_text.c_str());
        lv_obj_set_style_text_color(lbl, ColorUtils::invertColor(list_item_color), 0);

        auto* data = new FileListItemData(source, target_path, is_directory);
        lv_obj_set_user_data(btn, data);
        lv_obj_add_event_cb(btn, onFileSelected, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(btn, cleanup_file_button_user_data, LV_EVENT_DELETE, nullptr);
    };

    auto add_back_button = [&](StorageSource source, const std::string& path) {
        std::string label = ".. ";
        label += (path == "/") ? path : displayName(path);
        add_list_button(label, LV_SYMBOL_LEFT, source, path, true);
    };

    auto& sd = SdCardDriver::getInstance();
    add_section_label(LV_SYMBOL_SD_CARD " SD card", ColorUtils::invertColor(list_item_color));
    add_path_label(current_sd_path);
    if (!current_sd_path.empty() && current_sd_path != "/") {
        add_back_button(StorageSource::SdCard, parentPath(current_sd_path));
    }

    if (!sd.isMounted()) {
        add_message(LV_SYMBOL_WARNING " SD card not mounted");
    } else {
        auto entries = sd.listDirectory(current_sd_path.c_str(), MAX_STORAGE_ENTRIES);
        bool has_entries = false;
        bool has_audio = false;
        for (const auto& entry : entries) {
            if (entry.name.empty()) continue;

            has_entries = true;
            std::string entry_path = combinePath(current_sd_path, entry.name);
            std::string display = displayName(entry_path);
            if (entry.isDirectory) {
                add_list_button(display, LV_SYMBOL_DIRECTORY, StorageSource::SdCard, entry_path, true);
            } else if (hasAudioExtension(entry_path)) {
                add_list_button(display, LV_SYMBOL_AUDIO, StorageSource::SdCard, entry_path, false);
                has_audio = true;
            }
        }
        if (!has_entries) {
            add_message(LV_SYMBOL_FILE " Directory empty");
        } else if (!has_audio) {
            add_message(LV_SYMBOL_FILE " No audio files in this folder");
        }
    }

    add_section_label(LV_SYMBOL_DRIVE " Internal audio", ColorUtils::invertColor(list_item_color));
    add_path_label(current_littlefs_path);
    if (!current_littlefs_path.empty() && current_littlefs_path != "/") {
        add_back_button(StorageSource::LittleFS, parentPath(current_littlefs_path));
    }

    File dir = LittleFS.open(current_littlefs_path.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        add_message(LV_SYMBOL_WARNING " Unable to open directory");
    } else {
        bool has_entries = false;
        bool has_audio = false;
        File entry = dir.openNextFile();
        while (entry) {
            has_entries = true;
            std::string entry_path = combinePath(current_littlefs_path, entry.name());
            std::string display = displayName(entry_path);
            if (entry.isDirectory()) {
                add_list_button(display, LV_SYMBOL_DIRECTORY, StorageSource::LittleFS, entry_path, true);
            } else if (hasAudioExtension(entry_path)) {
                add_list_button(display, LV_SYMBOL_AUDIO, StorageSource::LittleFS, entry_path, false);
                has_audio = true;
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();
        if (!has_entries) {
            add_message(LV_SYMBOL_FILE " Directory empty");
        } else if (!has_audio) {
            add_message(LV_SYMBOL_FILE " No audio files in this folder");
        }
    }
}

void AudioPlayerScreen::navigateToDirectory(StorageSource source, const std::string& path) {
    std::string normalized = normalizePath(path);
    if (source == StorageSource::SdCard) {
        sd_current_path_ = normalized;
    } else {
        littlefs_current_path_ = normalized;
    }
    refreshFileList();
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
    if (!screen) return;
    lv_obj_t* btn = lv_event_get_target(event);
    auto* item = static_cast<FileListItemData*>(lv_obj_get_user_data(btn));
    if (!item) return;

    if (item->isDirectory) {
        Logger::getInstance().infof("[AudioPlayer] Directory tapped: source=%s path=%s",
                                   storageSourceToString(item->source),
                                   item->path.c_str());
        screen->navigateToDirectory(item->source, item->path);
        return;
    }

    std::string playback_path = item->path;
    if (item->source == StorageSource::SdCard) {
        playback_path = std::string("/sd") + playback_path;
    }

    screen->current_path_ = playback_path;
    AudioManager::getInstance().playFile(playback_path.c_str());
    screen->updatePlaybackInfo();

    Logger::getInstance().infof("[AudioPlayer] Selected file: %s", playback_path.c_str());
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

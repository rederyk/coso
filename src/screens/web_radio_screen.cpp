#include "screens/web_radio_screen.h"
#include "core/audio_manager.h"
#include "core/settings_manager.h"
#include "core/keyboard_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/color_utils.h"
#include <string>

namespace {
    constexpr uint32_t UPDATE_INTERVAL_MS = 1000;
    constexpr lv_coord_t DEFAULT_CARD_MIN_HEIGHT = 150;
    constexpr lv_coord_t CONTROLS_CARD_MIN_HEIGHT = 110;
    constexpr lv_coord_t LIST_CARD_MIN_HEIGHT = 220;

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
}

void WebRadioScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    const auto& settings = SettingsManager::getInstance().getSnapshot();
    lv_color_t primary_color = lv_color_hex(settings.primaryColor);
    lv_color_t accent_color = lv_color_hex(settings.accentColor);
    lv_color_t card_color = lv_color_hex(settings.cardColor);
    lv_color_t dock_color = lv_color_hex(settings.dockColor);
    lv_color_t primary_text = ColorUtils::invertColor(primary_color);
    lv_color_t card_text = ColorUtils::invertColor(card_color);
    lv_color_t muted_card = ColorUtils::getMutedTextColor(card_color);
    lv_color_t dock_text = ColorUtils::invertColor(dock_color);
    lv_color_t list_card_color = lv_color_mix(card_color, dock_color, LV_OPA_60);

    current_station_index_ = 0;

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
    lv_label_set_text(header, LV_SYMBOL_WIFI " Web Radio");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(header, accent_color, 0);

    // Now Playing Card
    lv_obj_t* now_playing_card = create_card(root, card_color, DEFAULT_CARD_MIN_HEIGHT);
    lv_obj_set_style_pad_all(now_playing_card, 16, 0);

    station_label = lv_label_create(now_playing_card);
    lv_label_set_text(station_label, "No station playing");
    lv_obj_set_width(station_label, lv_pct(100));
    lv_obj_set_style_text_font(station_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(station_label, card_text, 0);
    lv_label_set_long_mode(station_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    title_label = lv_label_create(now_playing_card);
    lv_label_set_text(title_label, "");
    lv_obj_set_width(title_label, lv_pct(100));
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, muted_card, 0);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    status_label = lv_label_create(now_playing_card);
    lv_label_set_text(status_label, LV_SYMBOL_OK " Ready");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, muted_card, 0);

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

    // Stations List Card
    lv_obj_t* list_card = create_card(root, list_card_color, LIST_CARD_MIN_HEIGHT);
    lv_obj_set_layout(list_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list_card, LV_FLEX_FLOW_COLUMN);

    // List header with add button
    lv_obj_t* list_header = lv_obj_create(list_card);
    lv_obj_remove_style_all(list_header);
    lv_obj_set_width(list_header, lv_pct(100));
    lv_obj_set_height(list_header, LV_SIZE_CONTENT);
    lv_obj_set_layout(list_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(list_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* list_title = lv_label_create(list_header);
    lv_label_set_text(list_title, LV_SYMBOL_LIST " Stations");
    lv_obj_set_style_text_font(list_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(list_title, ColorUtils::invertColor(list_card_color), 0);

    add_station_btn = lv_btn_create(list_header);
    lv_obj_set_size(add_station_btn, 80, 30);
    lv_obj_add_event_cb(add_station_btn, onAddStationClicked, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(add_station_btn, accent_color, 0);
    lv_obj_set_style_radius(add_station_btn, 12, 0);

    lv_obj_t* add_label = lv_label_create(add_station_btn);
    lv_label_set_text(add_label, LV_SYMBOL_PLUS " Add");
    lv_obj_center(add_label);
    ColorUtils::applyAutoButtonTextColor(add_station_btn);

    station_list = lv_obj_create(list_card);
    lv_obj_remove_style_all(station_list);
    lv_obj_set_width(station_list, lv_pct(100));
    lv_obj_set_height(station_list, LV_SIZE_CONTENT);
    lv_obj_set_layout(station_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(station_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(station_list, 10, 0);
    lv_obj_set_style_pad_column(station_list, 0, 0);
    lv_obj_clear_flag(station_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(station_list, lv_color_mix(list_card_color, primary_color, LV_OPA_20), 0);
    lv_obj_set_style_bg_opa(station_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(station_list, 8, 0);

    refreshStationList();
}

void WebRadioScreen::onShow() {
    auto& audio = AudioManager::getInstance();
    audio.setMetadataCallback(onMetadataCallback);

    // Start update timer
    if (!update_timer) {
        update_timer = lv_timer_create(onUpdateTimer, UPDATE_INTERVAL_MS, this);
    }

    updatePlaybackInfo();
}

void WebRadioScreen::onHide() {
    auto& audio = AudioManager::getInstance();
    audio.setMetadataCallback(nullptr);

    // Stop update timer
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = nullptr;
    }
}

void WebRadioScreen::refreshStationList() {
    if (!station_list) return;

    const auto& settings = SettingsManager::getInstance().getSnapshot();
    lv_color_t item_bg = lv_color_mix(lv_color_hex(settings.cardColor),
                                      lv_color_hex(settings.dockColor),
                                      LV_OPA_50);

    lv_obj_clean(station_list);

    auto& audio = AudioManager::getInstance();
    size_t num_stations = audio.getNumStations();

    for (size_t i = 0; i < num_stations; ++i) {
        const RadioStation* station = audio.getStation(i);
        if (!station) continue;

        std::string label_text = station->name;
        if (!station->genre.empty()) {
            label_text += " [" + station->genre + "]";
        }

        lv_obj_t* btn = lv_btn_create(station_list);
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
        style_list_button(btn, item_bg);

        lv_obj_t* icon = lv_label_create(btn);
        lv_label_set_text(icon, LV_SYMBOL_WIFI);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label_text.c_str());

        // Store station index in user data
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(i));
        lv_obj_add_event_cb(btn, onStationSelected, LV_EVENT_CLICKED, this);
    }

    if (num_stations == 0) {
        lv_obj_t* placeholder = lv_label_create(station_list);
        lv_label_set_text(placeholder, LV_SYMBOL_WARNING " No stations configured");
        lv_obj_set_style_text_color(placeholder, ColorUtils::invertColor(item_bg), 0);
    }
}

void WebRadioScreen::updatePlaybackInfo() {
    auto& audio = AudioManager::getInstance();
    const Metadata& meta = audio.getMetadata();

    if (audio.isPlaying() || audio.getState() == PlayerState::PAUSED) {
        const RadioStation* station = audio.getStation(current_station_index_);
        if (station) {
            lv_label_set_text(station_label, station->name.c_str());
        }

        if (meta.title[0] != '\0') {
            std::string info = meta.title.c_str();
            if (meta.artist[0] != '\0') {
                info += " - ";
                info += meta.artist.c_str();
            }
            lv_label_set_text(title_label, info.c_str());
        } else {
            lv_label_set_text(title_label, "Streaming...");
        }

        if (audio.getState() == PlayerState::PAUSED) {
            lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
            lv_label_set_text(status_label, LV_SYMBOL_PAUSE " Paused");
        } else {
            lv_label_set_text(play_pause_label, LV_SYMBOL_PAUSE);
            lv_label_set_text(status_label, LV_SYMBOL_OK " Streaming");
        }
    } else {
        lv_label_set_text(station_label, "No station playing");
        lv_label_set_text(title_label, "");
        lv_label_set_text(play_pause_label, LV_SYMBOL_PLAY);
        lv_label_set_text(status_label, LV_SYMBOL_OK " Ready");
    }
}

void WebRadioScreen::showAddStationDialog() {
    // TODO: Implement keyboard input dialog for adding custom stations
    // For now, just log
    Logger::getInstance().info("[WebRadio] Add station dialog - TODO: implement keyboard input");
}

// Event callbacks
void WebRadioScreen::onStationSelected(lv_event_t* event) {
    WebRadioScreen* screen = static_cast<WebRadioScreen*>(lv_event_get_user_data(event));
    lv_obj_t* btn = lv_event_get_target(event);

    size_t station_index = reinterpret_cast<size_t>(lv_obj_get_user_data(btn));
    screen->current_station_index_ = station_index;

    auto& audio = AudioManager::getInstance();
    const RadioStation* station = audio.getStation(station_index);

    if (station) {
        Logger::getInstance().infof("[WebRadio] Starting station: %s", station->name.c_str());

        lv_label_set_text(screen->status_label, LV_SYMBOL_REFRESH " Connecting...");

        if (audio.playRadioStation(station_index)) {
            screen->updatePlaybackInfo();
        } else {
            lv_label_set_text(screen->status_label, LV_SYMBOL_CLOSE " Connection failed");
        }
    }
}

void WebRadioScreen::onPlayPauseClicked(lv_event_t* event) {
    WebRadioScreen* screen = static_cast<WebRadioScreen*>(lv_event_get_user_data(event));
    AudioManager::getInstance().togglePause();
    screen->updatePlaybackInfo();
}

void WebRadioScreen::onStopClicked(lv_event_t* event) {
    WebRadioScreen* screen = static_cast<WebRadioScreen*>(lv_event_get_user_data(event));
    AudioManager::getInstance().stop();
    screen->updatePlaybackInfo();
}

void WebRadioScreen::onAddStationClicked(lv_event_t* event) {
    WebRadioScreen* screen = static_cast<WebRadioScreen*>(lv_event_get_user_data(event));
    screen->showAddStationDialog();
}

void WebRadioScreen::onVolumeChanged(lv_event_t* event) {
    lv_obj_t* slider = lv_event_get_target(event);
    int volume = lv_slider_get_value(slider);
    AudioManager::getInstance().setVolume(volume);
}

void WebRadioScreen::onUpdateTimer(lv_timer_t* timer) {
    WebRadioScreen* screen = static_cast<WebRadioScreen*>(timer->user_data);
    screen->updatePlaybackInfo();
}

void WebRadioScreen::onMetadataCallback(const Metadata& meta) {
    // Metadata will be updated on next timer tick
}

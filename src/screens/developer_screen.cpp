#include "screens/developer_screen.h"

#include "core/app_manager.h"
#include "core/settings_manager.h"
#include "utils/logger.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_chip_info.h>

DeveloperScreen::~DeveloperScreen() {
    if (stats_timer) {
        lv_timer_del(stats_timer);
        stats_timer = nullptr;
    }
    if (settings_listener_id) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void DeveloperScreen::build(lv_obj_t* parent) {
    auto& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 12, 0);
    lv_obj_set_style_pad_row(root, 12, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_outline_width(root, 0, 0);

    // Header
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 10, 0);

    back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 44, 44);
    lv_obj_add_event_cb(back_btn, handleBackButton, LV_EVENT_CLICKED, this);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    header_label = lv_label_create(header);
    lv_label_set_text(header_label, "Developer");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_24, 0);
    lv_obj_set_flex_grow(header_label, 1);
    lv_obj_set_style_text_align(header_label, LV_TEXT_ALIGN_CENTER, 0);

    // Spacer keeps the title centered in the flex row
    lv_obj_t* header_spacer = lv_obj_create(header);
    lv_obj_remove_style_all(header_spacer);
    lv_obj_set_size(header_spacer, 44, 44);

    // Scrollable content container
    content_container = lv_obj_create(root);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_width(content_container, LV_PCT(100));
    lv_obj_set_flex_grow(content_container, 1);
    lv_obj_set_layout(content_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content_container, 6, 0);
    lv_obj_set_style_pad_row(content_container, 12, 0);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_add_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content_container, LV_DIR_VER);

    // System Statistics Card
    stats_card = lv_obj_create(content_container);
    lv_obj_remove_style_all(stats_card);
    lv_obj_set_width(stats_card, LV_PCT(100));
    lv_obj_set_layout(stats_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(stats_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(stats_card, 14, 0);
    lv_obj_set_style_pad_row(stats_card, 6, 0);

    stats_title_label = lv_label_create(stats_card);
    lv_label_set_text(stats_title_label, "System Statistics");
    lv_obj_set_style_text_font(stats_title_label, &lv_font_montserrat_16, 0);

    stats_label = lv_label_create(stats_card);
    lv_label_set_long_mode(stats_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(stats_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(stats_label, LV_PCT(100));

    // Backup Status Card
    backup_card = lv_obj_create(content_container);
    lv_obj_remove_style_all(backup_card);
    lv_obj_set_width(backup_card, LV_PCT(100));
    lv_obj_set_layout(backup_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(backup_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(backup_card, 14, 0);
    lv_obj_set_style_pad_row(backup_card, 6, 0);

    backup_title_label = lv_label_create(backup_card);
    lv_label_set_text(backup_title_label, "Backup Status");
    lv_obj_set_style_text_font(backup_title_label, &lv_font_montserrat_16, 0);

    backup_status_label = lv_label_create(backup_card);
    lv_label_set_long_mode(backup_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(backup_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(backup_status_label, LV_PCT(100));

    // Actions Card
    actions_card = lv_obj_create(content_container);
    lv_obj_remove_style_all(actions_card);
    lv_obj_set_width(actions_card, LV_PCT(100));
    lv_obj_set_layout(actions_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actions_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(actions_card, 14, 0);
    lv_obj_set_style_pad_row(actions_card, 10, 0);

    actions_title_label = lv_label_create(actions_card);
    lv_label_set_text(actions_title_label, "Actions");
    lv_obj_set_style_text_font(actions_title_label, &lv_font_montserrat_16, 0);

    // Backup button
    backup_btn = lv_btn_create(actions_card);
    lv_obj_set_width(backup_btn, LV_PCT(100));
    lv_obj_set_height(backup_btn, 48);
    lv_obj_add_event_cb(backup_btn, handleBackupButton, LV_EVENT_CLICKED, this);

    lv_obj_t* backup_btn_label = lv_label_create(backup_btn);
    lv_label_set_text(backup_btn_label, "Backup to SD Card");
    lv_obj_center(backup_btn_label);

    // Restore button
    restore_btn = lv_btn_create(actions_card);
    lv_obj_set_width(restore_btn, LV_PCT(100));
    lv_obj_set_height(restore_btn, 48);
    lv_obj_add_event_cb(restore_btn, handleRestoreButton, LV_EVENT_CLICKED, this);

    lv_obj_t* restore_btn_label = lv_label_create(restore_btn);
    lv_label_set_text(restore_btn_label, "Restore from SD Card");
    lv_obj_center(restore_btn_label);

    // Reset settings button
    reset_btn = lv_btn_create(actions_card);
    lv_obj_set_width(reset_btn, LV_PCT(100));
    lv_obj_set_height(reset_btn, 48);
    lv_obj_add_event_cb(reset_btn, handleResetSettingsButton, LV_EVENT_CLICKED, this);

    lv_obj_t* reset_btn_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_btn_label, "Reset All Settings");
    lv_obj_center(reset_btn_label);

    // Apply theme
    applyThemeStyles(snapshot);

    // Update initial content
    updateStats();
    updateBackupStatus();

    // Register settings listener
    settings_listener_id = settings.addListener(
        [this](SettingsManager::SettingKey key, const SettingsSnapshot& snapshot) {
            applyThemeStyles(snapshot);
        });
}

void DeveloperScreen::onShow() {
    updateStats();
    updateBackupStatus();

    // Create timer to update stats every 2 seconds
    stats_timer = lv_timer_create(updateStatsTimer, 2000, this);
}

void DeveloperScreen::onHide() {
    if (stats_timer) {
        lv_timer_del(stats_timer);
        stats_timer = nullptr;
    }
}

void DeveloperScreen::updateStats() {
    auto& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    // Get system info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    // Calculate uptime
    uint32_t uptime_sec = millis() / 1000;
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;
    uint32_t seconds = uptime_sec % 60;

    // Get memory info
    uint32_t free_heap = ESP.getFreeHeap();
    uint32_t total_heap = ESP.getHeapSize();
    uint32_t free_psram = ESP.getFreePsram();
    uint32_t total_psram = ESP.getPsramSize();

    char stats_text[512];
    snprintf(stats_text, sizeof(stats_text),
             "Version: %s\n"
             "Boot Count: %u\n"
             "Hostname: %s\n"
             "Uptime: %02u:%02u:%02u\n\n"
             "Heap: %u KB / %u KB\n"
             "PSRAM: %u KB / %u KB\n"
             "CPU Cores: %d @ %d MHz",
             snapshot.version.c_str(),
             snapshot.bootCount,
             snapshot.hostname.c_str(),
             hours, minutes, seconds,
             free_heap / 1024, total_heap / 1024,
             free_psram / 1024, total_psram / 1024,
             chip_info.cores,
             ESP.getCpuFreqMHz());

    lv_label_set_text(stats_label, stats_text);
}

void DeveloperScreen::updateBackupStatus() {
    auto& settings = SettingsManager::getInstance();
    bool has_backup = settings.hasBackup();
    bool sd_mounted = SD_MMC.cardType() != CARD_NONE;

    char status_text[256];
    if (!sd_mounted) {
        snprintf(status_text, sizeof(status_text), "SD Card: Not mounted");
    } else {
        uint64_t card_size = SD_MMC.cardSize() / (1024 * 1024);
        uint64_t used_size = SD_MMC.usedBytes() / (1024 * 1024);
        snprintf(status_text, sizeof(status_text),
                 "SD Card: OK (%llu MB used / %llu MB total)\n"
                 "Backup Available: %s\n"
                 "Last Backup: %s",
                 used_size, card_size,
                 has_backup ? "Yes" : "No",
                 has_backup ? settings.getLastBackupTime().c_str() : "Never");
    }

    lv_label_set_text(backup_status_label, status_text);
}

void DeveloperScreen::applyThemeStyles(const SettingsSnapshot& snapshot) {
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t card = lv_color_hex(snapshot.cardColor);
    lv_color_t card_tint = lv_color_mix(accent, card, LV_OPA_40);
    lv_color_t text = lv_color_hex(0xffffff);
    lv_color_t subtle_text = lv_color_mix(accent, text, LV_OPA_30);
    lv_color_t shadow = lv_color_mix(accent, lv_color_hex(0x000000), LV_OPA_50);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    }
    if (header_label) {
        lv_obj_set_style_text_color(header_label, accent, 0);
    }

    if (back_btn) {
        lv_obj_set_style_bg_color(back_btn, card_tint, 0);
        lv_obj_set_style_radius(back_btn, snapshot.borderRadius, 0);
        lv_obj_set_style_border_width(back_btn, 0, 0);
        lv_obj_set_style_text_color(back_btn, text, 0);
    }

    auto style_card = [&](lv_obj_t* card_obj) {
        if (!card_obj) {
            return;
        }
        lv_obj_set_style_bg_color(card_obj, card_tint, 0);
        lv_obj_set_style_bg_opa(card_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card_obj, snapshot.borderRadius, 0);
        lv_obj_set_style_border_width(card_obj, 0, 0);
        lv_obj_set_style_shadow_width(card_obj, 10, 0);
        lv_obj_set_style_shadow_spread(card_obj, 2, 0);
        lv_obj_set_style_shadow_color(card_obj, shadow, 0);
        lv_obj_set_style_shadow_opa(card_obj, LV_OPA_30, 0);
    };

    style_card(stats_card);
    style_card(backup_card);
    style_card(actions_card);

    if (stats_title_label) {
        lv_obj_set_style_text_color(stats_title_label, accent, 0);
    }
    if (backup_title_label) {
        lv_obj_set_style_text_color(backup_title_label, accent, 0);
    }
    if (actions_title_label) {
        lv_obj_set_style_text_color(actions_title_label, accent, 0);
    }
    if (stats_label) {
        lv_obj_set_style_text_color(stats_label, subtle_text, 0);
    }
    if (backup_status_label) {
        lv_obj_set_style_text_color(backup_status_label, subtle_text, 0);
    }

    lv_color_t primary_button = lv_color_mix(accent, primary, LV_OPA_80);
    lv_color_t subtle_button = lv_color_mix(accent, card, LV_OPA_60);
    lv_color_t danger_button = lv_color_mix(lv_color_hex(0xff4d4f), accent, LV_OPA_60);
    auto style_button = [&](lv_obj_t* button, lv_color_t bg) {
        if (!button) {
            return;
        }
        lv_obj_set_style_bg_color(button, bg, 0);
        lv_obj_set_style_radius(button, snapshot.borderRadius, 0);
        lv_obj_set_style_border_width(button, 0, 0);
        lv_obj_set_style_text_color(button, text, 0);
    };

    style_button(backup_btn, primary_button);
    style_button(restore_btn, subtle_button);
    style_button(reset_btn, danger_button);
}

void DeveloperScreen::handleBackButton(lv_event_t* e) {
    auto* screen = static_cast<DeveloperScreen*>(e->user_data);
    if (!screen) return;

    Logger::getInstance().info("[Developer] Returning to Settings...");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("settings");
    }
}

void DeveloperScreen::handleBackupButton(lv_event_t* e) {
    auto* screen = static_cast<DeveloperScreen*>(e->user_data);
    auto& settings = SettingsManager::getInstance();
    auto& logger = Logger::getInstance();

    if (settings.backupToSD()) {
        logger.info("[Developer] Backup to SD card successful");
        screen->updateBackupStatus();
    } else {
        logger.error("[Developer] Backup to SD card failed");
    }
}

void DeveloperScreen::handleRestoreButton(lv_event_t* e) {
    auto* screen = static_cast<DeveloperScreen*>(e->user_data);
    auto& settings = SettingsManager::getInstance();
    auto& logger = Logger::getInstance();

    if (settings.restoreFromSD()) {
        logger.info("[Developer] Restore from SD card successful");
        screen->updateStats();
        screen->updateBackupStatus();
    } else {
        logger.error("[Developer] Restore from SD card failed");
    }
}

void DeveloperScreen::handleResetSettingsButton(lv_event_t* e) {
    auto& settings = SettingsManager::getInstance();
    auto& logger = Logger::getInstance();

    logger.warn("[Developer] Resetting all settings to defaults");
    settings.reset();

    auto* screen = static_cast<DeveloperScreen*>(e->user_data);
    screen->updateStats();
    screen->updateBackupStatus();
}

void DeveloperScreen::updateStatsTimer(lv_timer_t* timer) {
    auto* screen = static_cast<DeveloperScreen*>(timer->user_data);
    screen->updateStats();
}

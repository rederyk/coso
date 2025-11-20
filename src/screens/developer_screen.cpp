#include "screens/developer_screen.h"

#include "core/app_manager.h"
#include "core/settings_manager.h"
#include "core/theme_palette.h"
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

    // Header
    header_label = lv_label_create(parent);
    lv_label_set_text(header_label, "Developer");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(header_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(header_label, LV_ALIGN_TOP_MID, 0, 10);

    // Back button
    back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 60, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 8);
    lv_obj_add_event_cb(back_btn, handleBackButton, LV_EVENT_CLICKED, this);

    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Scrollable content container
    content_container = lv_obj_create(parent);
    lv_obj_set_size(content_container, LV_PCT(100), LV_PCT(100) - 60);
    lv_obj_align(content_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_style_pad_all(content_container, 10, 0);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content_container, 10, 0);

    // System Statistics Card
    stats_card = lv_obj_create(content_container);
    lv_obj_set_size(stats_card, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(stats_card, 15, 0);

    lv_obj_t* stats_title = lv_label_create(stats_card);
    lv_label_set_text(stats_title, "System Statistics");
    lv_obj_set_style_text_font(stats_title, &lv_font_montserrat_16, 0);
    lv_obj_align(stats_title, LV_ALIGN_TOP_LEFT, 0, 0);

    stats_label = lv_label_create(stats_card);
    lv_obj_set_style_text_font(stats_label, &lv_font_montserrat_14, 0);
    lv_obj_align(stats_label, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_width(stats_label, LV_PCT(100));

    // Backup Status Card
    backup_card = lv_obj_create(content_container);
    lv_obj_set_size(backup_card, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(backup_card, 15, 0);

    lv_obj_t* backup_title = lv_label_create(backup_card);
    lv_label_set_text(backup_title, "Backup Status");
    lv_obj_set_style_text_font(backup_title, &lv_font_montserrat_16, 0);
    lv_obj_align(backup_title, LV_ALIGN_TOP_LEFT, 0, 0);

    backup_status_label = lv_label_create(backup_card);
    lv_obj_set_style_text_font(backup_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(backup_status_label, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_width(backup_status_label, LV_PCT(100));

    // Actions Card
    actions_card = lv_obj_create(content_container);
    lv_obj_set_size(actions_card, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(actions_card, 15, 0);

    lv_obj_t* actions_title = lv_label_create(actions_card);
    lv_label_set_text(actions_title, "Actions");
    lv_obj_set_style_text_font(actions_title, &lv_font_montserrat_16, 0);
    lv_obj_align(actions_title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Backup button
    lv_obj_t* backup_btn = lv_btn_create(actions_card);
    lv_obj_set_size(backup_btn, LV_PCT(95), 50);
    lv_obj_align(backup_btn, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_add_event_cb(backup_btn, handleBackupButton, LV_EVENT_CLICKED, this);

    lv_obj_t* backup_btn_label = lv_label_create(backup_btn);
    lv_label_set_text(backup_btn_label, "Backup to SD Card");
    lv_obj_center(backup_btn_label);

    // Restore button
    lv_obj_t* restore_btn = lv_btn_create(actions_card);
    lv_obj_set_size(restore_btn, LV_PCT(95), 50);
    lv_obj_align(restore_btn, LV_ALIGN_TOP_MID, 0, 95);
    lv_obj_add_event_cb(restore_btn, handleRestoreButton, LV_EVENT_CLICKED, this);

    lv_obj_t* restore_btn_label = lv_label_create(restore_btn);
    lv_label_set_text(restore_btn_label, "Restore from SD Card");
    lv_obj_center(restore_btn_label);

    // Reset settings button
    lv_obj_t* reset_btn = lv_btn_create(actions_card);
    lv_obj_set_size(reset_btn, LV_PCT(95), 50);
    lv_obj_align(reset_btn, LV_ALIGN_TOP_MID, 0, 155);
    lv_obj_add_event_cb(reset_btn, handleResetSettingsButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xff0000), 0);

    lv_obj_t* reset_btn_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_btn_label, "Reset All Settings");
    lv_obj_center(reset_btn_label);

    // Update card height
    lv_obj_set_height(actions_card, 220);

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
    lv_obj_set_style_text_color(header_label, lv_color_hex(0xffffff), 0);

    // Style back button
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(snapshot.cardColor), 0);
    lv_obj_set_style_radius(back_btn, snapshot.borderRadius, 0);

    // Style cards
    lv_obj_set_style_bg_color(stats_card, lv_color_hex(snapshot.cardColor), 0);
    lv_obj_set_style_radius(stats_card, snapshot.borderRadius, 0);
    lv_obj_set_style_border_width(stats_card, 0, 0);

    lv_obj_set_style_bg_color(backup_card, lv_color_hex(snapshot.cardColor), 0);
    lv_obj_set_style_radius(backup_card, snapshot.borderRadius, 0);
    lv_obj_set_style_border_width(backup_card, 0, 0);

    lv_obj_set_style_bg_color(actions_card, lv_color_hex(snapshot.cardColor), 0);
    lv_obj_set_style_radius(actions_card, snapshot.borderRadius, 0);
    lv_obj_set_style_border_width(actions_card, 0, 0);
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

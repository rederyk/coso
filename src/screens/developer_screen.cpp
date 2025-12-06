#include "screens/developer_screen.h"

#include "core/app_manager.h"
#include "core/ble_hid_manager.h"
#include "screens/ble_manager.h"
#include "core/settings_manager.h"
#include "utils/logger.h"
#include "lvgl_power_manager.h"
#include <esp_heap_caps.h>
#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_chip_info.h>
#include <lvgl.h>
#include <NimBLEDevice.h>

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
    lv_obj_set_height(content_container, LV_SIZE_CONTENT); // Fix: ensure card wraps content


    // System Statistics Card
    stats_card = lv_obj_create(content_container);
    lv_obj_remove_style_all(stats_card);
    lv_obj_set_width(stats_card, LV_PCT(100));
    lv_obj_set_layout(stats_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(stats_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(stats_card, 14, 0);
    lv_obj_set_style_pad_row(stats_card, 6, 0);
        lv_obj_set_height(stats_card, LV_SIZE_CONTENT); // Fix: ensure card wraps content

    

    stats_title_label = lv_label_create(stats_card);
    lv_label_set_text(stats_title_label, "System Statistics");
    lv_obj_set_style_text_font(stats_title_label, &lv_font_montserrat_16, 0);

    stats_label = lv_label_create(stats_card);
    lv_label_set_long_mode(stats_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(stats_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(stats_label, LV_PCT(100));

    // Memory & Storage Card
    memory_card = lv_obj_create(content_container);
    lv_obj_remove_style_all(memory_card);
    lv_obj_set_width(memory_card, LV_PCT(100));
    lv_obj_set_layout(memory_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(memory_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(memory_card, 14, 0);
    lv_obj_set_style_pad_row(memory_card, 6, 0);
    lv_obj_set_height(memory_card, LV_SIZE_CONTENT); // Fix: ensure card wraps content


    memory_title_label = lv_label_create(memory_card);
    lv_label_set_text(memory_title_label, "Memory & Storage");
    lv_obj_set_style_text_font(memory_title_label, &lv_font_montserrat_16, 0);

    memory_label = lv_label_create(memory_card);
    lv_label_set_long_mode(memory_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(memory_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(memory_label, LV_PCT(100));

    backup_status_label = lv_label_create(memory_card);
    lv_label_set_long_mode(backup_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(backup_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(backup_status_label, LV_PCT(100));

    memory_help_label = lv_label_create(memory_card);
    lv_label_set_long_mode(memory_help_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(memory_help_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(memory_help_label, LV_PCT(100));

    // Free PSRAM button
    free_psram_btn = lv_btn_create(memory_card);
    lv_obj_set_width(free_psram_btn, LV_PCT(100));
    lv_obj_set_height(free_psram_btn, 44);
    lv_obj_add_event_cb(free_psram_btn, handleFreePsramButton, LV_EVENT_CLICKED, this);
    lv_obj_t* free_psram_label = lv_label_create(free_psram_btn);
    lv_label_set_text(free_psram_label, "Libera PSRAM (cache LVGL)");
    lv_obj_center(free_psram_label);

    // Free DRAM button
    free_dram_btn = lv_btn_create(memory_card);
    lv_obj_set_width(free_dram_btn, LV_PCT(100));
    lv_obj_set_height(free_dram_btn, 44);
    lv_obj_add_event_cb(free_dram_btn, handleFreeDramButton, LV_EVENT_CLICKED, this);
    lv_obj_t* free_dram_label = lv_label_create(free_dram_btn);
    lv_label_set_text(free_dram_label, "Libera DRAM (heap interno)");
    lv_obj_center(free_dram_label);

    // Backup button
    backup_btn = lv_btn_create(memory_card);
    lv_obj_set_width(backup_btn, LV_PCT(100));
    lv_obj_set_height(backup_btn, 48);
    lv_obj_add_event_cb(backup_btn, handleBackupButton, LV_EVENT_CLICKED, this);
    lv_obj_t* backup_btn_label = lv_label_create(backup_btn);
    lv_label_set_text(backup_btn_label, "Backup to SD Card");
    lv_obj_center(backup_btn_label);

    // Restore button
    restore_btn = lv_btn_create(memory_card);
    lv_obj_set_width(restore_btn, LV_PCT(100));
    lv_obj_set_height(restore_btn, 48);
    lv_obj_add_event_cb(restore_btn, handleRestoreButton, LV_EVENT_CLICKED, this);
    lv_obj_t* restore_btn_label = lv_label_create(restore_btn);
    lv_label_set_text(restore_btn_label, "Restore from SD Card");
    lv_obj_center(restore_btn_label);

    // TEST: LVGL Power Manager buttons
    lv_obj_t* power_card = lv_obj_create(content_container);
    lv_obj_remove_style_all(power_card);
    lv_obj_set_width(power_card, LV_PCT(100));
    lv_obj_set_layout(power_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(power_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(power_card, 14, 0);
    lv_obj_set_style_pad_row(power_card, 6, 0);
    lv_obj_set_height(power_card, LV_SIZE_CONTENT);

    lv_obj_t* power_title = lv_label_create(power_card);
    lv_label_set_text(power_title, "LVGL Power Manager TEST");
    lv_obj_set_style_text_font(power_title, &lv_font_montserrat_16, 0);

    // Suspend LVGL button
    lv_obj_t* suspend_btn = lv_btn_create(power_card);
    lv_obj_set_width(suspend_btn, LV_PCT(100));
    lv_obj_set_height(suspend_btn, 48);
    lv_obj_add_event_cb(suspend_btn, [](lv_event_t* e) {
        auto& log = Logger::getInstance();
        log.info("[TEST] Suspending LVGL...");
        LVGLPowerMgr.printMemoryStats();
        LVGLPowerMgr.switchToVoiceMode();
        log.info("[TEST] LVGL suspended. Screen should be black. Touch to resume.");
        LVGLPowerMgr.printMemoryStats();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* suspend_label = lv_label_create(suspend_btn);
    lv_label_set_text(suspend_label, "TEST: Suspend LVGL (Free ~100KB)");
    lv_obj_center(suspend_label);

    // Resume LVGL button
    lv_obj_t* resume_btn = lv_btn_create(power_card);
    lv_obj_set_width(resume_btn, LV_PCT(100));
    lv_obj_set_height(resume_btn, 48);
    lv_obj_add_event_cb(resume_btn, [](lv_event_t* e) {
        auto& log = Logger::getInstance();
        log.info("[TEST] Resuming LVGL...");
        LVGLPowerMgr.switchToUIMode();
        log.info("[TEST] LVGL resumed");
        LVGLPowerMgr.printMemoryStats();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* resume_label = lv_label_create(resume_btn);
    lv_label_set_text(resume_label, "TEST: Resume LVGL");
    lv_obj_center(resume_label);

    // Detailed DRAM analysis button
    lv_obj_t* dram_analysis_btn = lv_btn_create(power_card);
    lv_obj_set_width(dram_analysis_btn, LV_PCT(100));
    lv_obj_set_height(dram_analysis_btn, 48);
    lv_obj_add_event_cb(dram_analysis_btn, [](lv_event_t* e) {
        auto& log = Logger::getInstance();
        log.info("[DRAM] Analyzing DRAM usage...");

        // Force print to serial using Logger instead of ESP_LOGI
        uint32_t free_dram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t total_dram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
        uint32_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        uint32_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);

        log.info("=== Detailed DRAM Usage ===");
        log.info("DRAM Heap:");
        log.infof("  Total:         %6lu KB", total_dram / 1024);
        log.infof("  Free:          %6lu KB (%.1f%%)",
                 free_dram / 1024, (float)free_dram / total_dram * 100.0f);
        log.infof("  Used:          %6lu KB", (total_dram - free_dram) / 1024);
        log.infof("  Largest block: %6lu KB", largest_block / 1024);
        log.infof("  Min free ever: %6lu KB", min_free / 1024);
        log.infof("  Fragmentation: %.1f%%",
                 100.0f * (1.0f - (float)largest_block / free_dram));

        log.info("Task Stacks (approx):");
        log.info("  LVGL task:     ~8-12 KB");
        log.info("  Network task:  ~8-12 KB");
        log.info("  Audio task:    ~4-8 KB");
        log.info("  Other tasks:   ~20-30 KB");

        uint32_t estimated_stacks = 50 * 1024;
        uint32_t estimated_used = total_dram - free_dram;
        uint32_t heap_allocated = estimated_used - estimated_stacks;

        log.info("Estimated breakdown:");
        log.infof("  Task stacks:   ~%6lu KB (fixed)", estimated_stacks / 1024);
        log.infof("  Heap allocs:   ~%6lu KB (dynamic)", heap_allocated / 1024);
        log.infof("  Free:           %6lu KB", free_dram / 1024);

        log.info("Note: LVGL draw buffer is in PSRAM (mode 0), not using DRAM");
        log.info("===========================");
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* dram_analysis_label = lv_label_create(dram_analysis_btn);
    lv_label_set_text(dram_analysis_label, "Analyze DRAM Usage");
    lv_obj_center(dram_analysis_label);

    memory_result_label = lv_label_create(memory_card);
    lv_label_set_long_mode(memory_result_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(memory_result_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(memory_result_label, LV_PCT(100));
    lv_label_set_text(memory_result_label, "Cache non svuotate ancora.");



    
    // System Controls card
    controls_card = lv_obj_create(content_container);
    lv_obj_remove_style_all(controls_card);
    lv_obj_set_width(controls_card, LV_PCT(100));
    lv_obj_set_layout(controls_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(controls_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(controls_card, 14, 0);
    lv_obj_set_style_pad_row(controls_card, 10, 0);
    lv_obj_set_height(controls_card, LV_SIZE_CONTENT); // Fix: ensure card wraps content


    controls_title_label = lv_label_create(controls_card);
    lv_label_set_text(controls_title_label, "System Controls");
    lv_obj_set_style_text_font(controls_title_label, &lv_font_montserrat_16, 0);

    // Reset button
    reset_btn = lv_btn_create(controls_card);
    lv_obj_set_width(reset_btn, LV_PCT(100));
    lv_obj_set_height(reset_btn, 48);
    lv_obj_add_event_cb(reset_btn, handleResetButton, LV_EVENT_CLICKED, this);
    lv_obj_t* reset_btn_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_btn_label, LV_SYMBOL_REFRESH " Reset Settings");
    lv_obj_center(reset_btn_label);

    // Reboot button
    reboot_btn = lv_btn_create(controls_card);
    lv_obj_set_width(reboot_btn, LV_PCT(100));
    lv_obj_set_height(reboot_btn, 48);
    lv_obj_add_event_cb(reboot_btn, handleRebootButton, LV_EVENT_CLICKED, this);
    lv_obj_t* reboot_btn_label = lv_label_create(reboot_btn);
    lv_label_set_text(reboot_btn_label, LV_SYMBOL_POWER " Reboot System");
    lv_obj_center(reboot_btn_label);

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

    // Get DRAM / PSRAM info
    size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t dram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t dram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    size_t dram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    char stats_text[512];
    snprintf(stats_text, sizeof(stats_text),
             "Version: %s\n"
             "Boot Count: %u\n"
             "Hostname: %s\n"
             "Uptime: %02u:%02u:%02u\n"
             "CPU Cores: %d @ %d MHz",
             snapshot.version.c_str(),
             snapshot.bootCount,
             snapshot.hostname.c_str(),
             hours, minutes, seconds,
             chip_info.cores,
             ESP.getCpuFreqMHz());

    lv_label_set_text(stats_label, stats_text);
    updateMemoryInfo();
}

void DeveloperScreen::updateMemoryInfo() {
    if (!memory_label) {
        return;
    }

    size_t dram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t dram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    size_t dram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    char memory_text[512];
    if (psram_total == 0) {
        snprintf(memory_text, sizeof(memory_text),
                 "DRAM: %u/%u KB liberi (min %u KB, blocco max %u KB)\n"
                 "PSRAM: non rilevata",
                 static_cast<unsigned>(dram_free / 1024), static_cast<unsigned>(dram_total / 1024),
                 static_cast<unsigned>(dram_min / 1024), static_cast<unsigned>(dram_largest / 1024));
    } else {
        snprintf(memory_text, sizeof(memory_text),
                 "DRAM (interna): %u/%u KB liberi\n"
                 "  Min: %u KB | Blocco max: %u KB\n"
                 "PSRAM (esterna): %u/%u KB liberi\n"
                 "  Min: %u KB | Blocco max: %u KB\n"
                 ,
                 static_cast<unsigned>(dram_free / 1024), static_cast<unsigned>(dram_total / 1024),
                 static_cast<unsigned>(dram_min / 1024), static_cast<unsigned>(dram_largest / 1024),
                 static_cast<unsigned>(psram_free / 1024), static_cast<unsigned>(psram_total / 1024),
                 static_cast<unsigned>(psram_min / 1024), static_cast<unsigned>(psram_largest / 1024));
    }

    lv_label_set_text(memory_label, memory_text);
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
    style_card(memory_card);
    style_card(controls_card);

    if (stats_title_label) {
        lv_obj_set_style_text_color(stats_title_label, accent, 0);
    }
    if (memory_title_label) {
        lv_obj_set_style_text_color(memory_title_label, accent, 0);
    }
    if (controls_title_label) {
        lv_obj_set_style_text_color(controls_title_label, accent, 0);
    }
    if (stats_label) {
        lv_obj_set_style_text_color(stats_label, subtle_text, 0);
    }
    if (memory_label) {
        lv_obj_set_style_text_color(memory_label, subtle_text, 0);
    }
    if (memory_help_label) {
        lv_obj_set_style_text_color(memory_help_label, subtle_text, 0);
    }
    if (memory_result_label) {
        lv_obj_set_style_text_color(memory_result_label, subtle_text, 0);
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
    style_button(free_psram_btn, primary_button);
    style_button(free_dram_btn, subtle_button);
    style_button(reset_btn, danger_button);
    style_button(reboot_btn, danger_button);
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
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    static const char* backup_btns[] = {"Annulla", "Backup", ""};
    lv_obj_t* mbox = lv_msgbox_create(NULL, "Conferma Backup",
        "Eseguire il backup delle impostazioni sulla scheda SD?",
        backup_btns, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, confirmBackup, LV_EVENT_VALUE_CHANGED, screen);
}

void DeveloperScreen::handleRestoreButton(lv_event_t* e) {
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    static const char* restore_btns[] = {"Annulla", "Ripristina", ""};
    lv_obj_t* mbox = lv_msgbox_create(NULL, "Conferma Ripristino",
        "Ripristinare le impostazioni dalla scheda SD?\n\nL'operazione sovrascriverà le impostazioni correnti.",
        restore_btns, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, confirmRestore, LV_EVENT_VALUE_CHANGED, screen);
}

void DeveloperScreen::handleFreePsramButton(lv_event_t* e) {
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    screen->runMemoryCleanup(true);
}

void DeveloperScreen::handleFreeDramButton(lv_event_t* e) {
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    screen->runMemoryCleanup(false);
}

void DeveloperScreen::updateStatsTimer(lv_timer_t* timer) {
    auto* screen = static_cast<DeveloperScreen*>(timer->user_data);
    screen->updateStats();
}

void DeveloperScreen::confirmBackup(lv_event_t* e) {
    lv_obj_t* mbox = lv_event_get_current_target(e);
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    uint16_t btn_id = lv_msgbox_get_active_btn(mbox);

    if (btn_id == 1) { // "Backup" button
        auto& settings = SettingsManager::getInstance();
        auto& logger = Logger::getInstance();

        if (settings.backupToSD()) {
            logger.info("[Developer] Backup to SD card successful");
            if (screen) {
                screen->updateBackupStatus();
            }
        } else {
            logger.error("[Developer] Backup to SD card failed");
        }
    }

    lv_msgbox_close(mbox);
}

void DeveloperScreen::confirmRestore(lv_event_t* e) {
    lv_obj_t* mbox = lv_event_get_current_target(e);
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    uint16_t btn_id = lv_msgbox_get_active_btn(mbox);

    if (btn_id == 1) { // "Ripristina" button
        auto& settings = SettingsManager::getInstance();
        auto& logger = Logger::getInstance();

        if (settings.restoreFromSD()) {
            logger.info("[Developer] Restore from SD card successful");
            if (screen) {
                screen->updateStats();
                screen->updateBackupStatus();
            }
        } else {
            logger.error("[Developer] Restore from SD card failed");
        }
    }

    lv_msgbox_close(mbox);
}

void DeveloperScreen::runMemoryCleanup(bool target_psram) {
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (target_psram && psram_total == 0) {
        if (memory_result_label) {
            lv_label_set_text(memory_result_label, "PSRAM non rilevata");
        }
        Logger::getInstance().warn("[Developer] PSRAM cleanup richiesto ma non disponibile");
        return;
    }

    size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t dram_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // Svuota cache LVGL e buffer temporanei allocati su PSRAM/DRAM.
    lv_mem_buf_free_all();
    lv_img_cache_invalidate_src(nullptr);
    Logger::getInstance().clearBuffer();

    size_t psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t dram_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    int32_t psram_delta = static_cast<int32_t>(psram_after) - static_cast<int32_t>(psram_before);
    int32_t dram_delta = static_cast<int32_t>(dram_after) - static_cast<int32_t>(dram_before);

    char result[160];
    if (target_psram) {
        snprintf(result, sizeof(result),
                 "PSRAM liberata: %+d KB (cache LVGL svuotate)",
                 static_cast<int>(psram_delta / 1024));
        Logger::getInstance().infof("[Developer] PSRAM cleanup delta: %d bytes", psram_delta);
    } else {
        snprintf(result, sizeof(result),
                 "DRAM liberata: %+d KB (heap interno)",
                 static_cast<int>(dram_delta / 1024));
        Logger::getInstance().infof("[Developer] DRAM cleanup delta: %d bytes", dram_delta);
    }

    if (memory_result_label) {
        lv_label_set_text(memory_result_label, result);
    }

    updateStats();
}

void DeveloperScreen::handleResetButton(lv_event_t* e) {
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    // Create confirmation dialog
    static const char* reset_btns[] = {"Annulla", "Reset", ""};
    lv_obj_t* mbox = lv_msgbox_create(NULL, "Conferma Reset",
        "Ripristinare le impostazioni ai valori predefiniti?\n\nQuesta operazione è irreversibile.",
        reset_btns, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, confirmReset, LV_EVENT_VALUE_CHANGED, screen);
}

void DeveloperScreen::handleRebootButton(lv_event_t* e) {
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    // Create confirmation dialog
    static const char* reboot_btns[] = {"Annulla", "Riavvia", ""};
    lv_obj_t* mbox = lv_msgbox_create(NULL, "Conferma Riavvio",
        "Riavviare il sistema?\n\nTutte le impostazioni saranno salvate.",
        reboot_btns, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, confirmReboot, LV_EVENT_VALUE_CHANGED, screen);
}

void DeveloperScreen::confirmReset(lv_event_t* e) {
    lv_obj_t* mbox = lv_event_get_current_target(e);
    auto* screen = static_cast<DeveloperScreen*>(lv_event_get_user_data(e));
    uint16_t btn_id = lv_msgbox_get_active_btn(mbox);

    if (btn_id == 1) { // "Reset" button
        Logger::getInstance().warn("[System] Resetting to defaults...");
        SettingsManager::getInstance().reset();
        // Disconnetti qualsiasi host BLE collegato e cancella i bond
        BleHidManager& ble = BleHidManager::getInstance();
        BleManager::getInstance().disconnectAll();
        NimBLEDevice::deleteAllBonds();
        if (ble.isInitialized() && ble.isEnabled() && !ble.isConnected()) {
            BleManager::getInstance().startAdvertising();
        }
        Logger::getInstance().info("[BLE] Bonding cancellati e connessioni chiuse dopo reset");
        Logger::getInstance().info("[System] Reset complete");

        if (screen) {
            screen->updateStats();
            screen->updateBackupStatus();
        }
    }

    lv_msgbox_close(mbox);
}

void DeveloperScreen::confirmReboot(lv_event_t* e) {
    lv_obj_t* mbox = lv_event_get_current_target(e);
    uint16_t btn_id = lv_msgbox_get_active_btn(mbox);

    if (btn_id == 1) { // "Riavvia" button
        Logger::getInstance().warn("[System] Rebooting...");
        lv_msgbox_close(mbox);
        delay(500);
        ESP.restart();
    }

    lv_msgbox_close(mbox);
}

#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"

class DeveloperScreen : public Screen {
public:
    ~DeveloperScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void updateStats();
    void updateBackupStatus();
    void updateMemoryInfo();
    void runMemoryCleanup(bool target_psram);
    void applyThemeStyles(const SettingsSnapshot& snapshot);

    static void handleBackButton(lv_event_t* e);
    static void handleBackupButton(lv_event_t* e);
    static void handleRestoreButton(lv_event_t* e);
    static void handleFreePsramButton(lv_event_t* e);
    static void handleFreeDramButton(lv_event_t* e);
    static void updateStatsTimer(lv_timer_t* timer);
    static void handleResetButton(lv_event_t* e);
    static void handleRebootButton(lv_event_t* e);
    static void confirmBackup(lv_event_t* e);
    static void confirmRestore(lv_event_t* e);
    static void confirmReset(lv_event_t* e);
    static void confirmReboot(lv_event_t* e);
    lv_obj_t* back_btn = nullptr;
    lv_obj_t* header_label = nullptr;
    lv_obj_t* content_container = nullptr;
    lv_obj_t* stats_card = nullptr;
    lv_obj_t* stats_title_label = nullptr;
    lv_obj_t* stats_label = nullptr;
    lv_obj_t* memory_card = nullptr;
    lv_obj_t* memory_title_label = nullptr;
    lv_obj_t* memory_label = nullptr;
    lv_obj_t* backup_status_label = nullptr;
    lv_obj_t* memory_help_label = nullptr;
    lv_obj_t* memory_result_label = nullptr;
    lv_obj_t* free_psram_btn = nullptr;
    lv_obj_t* free_dram_btn = nullptr;
    lv_obj_t* backup_btn = nullptr;
    lv_obj_t* restore_btn = nullptr;
    lv_obj_t* reset_btn = nullptr;
    lv_obj_t* reboot_btn = nullptr;
    lv_obj_t* controls_card = nullptr;
    lv_obj_t* controls_title_label = nullptr;

    lv_timer_t* stats_timer = nullptr;
    uint32_t settings_listener_id = 0;
};

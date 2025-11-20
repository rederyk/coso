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
    void applyThemeStyles(const SettingsSnapshot& snapshot);

    static void handleBackButton(lv_event_t* e);
    static void handleBackupButton(lv_event_t* e);
    static void handleRestoreButton(lv_event_t* e);
    static void handleResetSettingsButton(lv_event_t* e);
    static void updateStatsTimer(lv_timer_t* timer);

    lv_obj_t* back_btn = nullptr;
    lv_obj_t* header_label = nullptr;
    lv_obj_t* content_container = nullptr;
    lv_obj_t* stats_card = nullptr;
    lv_obj_t* stats_label = nullptr;
    lv_obj_t* backup_card = nullptr;
    lv_obj_t* backup_status_label = nullptr;
    lv_obj_t* actions_card = nullptr;

    lv_timer_t* stats_timer = nullptr;
    uint32_t settings_listener_id = 0;
};

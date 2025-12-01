#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"
#include "utils/logger.h"
#include <Arduino.h>
#include <lvgl.h>

class SystemLogScreen : public Screen {
public:
    ~SystemLogScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void refreshLogView();
    void applyTheme(const SettingsSnapshot& snapshot);
    void attachSettingsListener();
    void detachSettingsListener();
    void updateFilterButtons();

    static void timerCallback(lv_timer_t* timer);
    static void clearEventHandler(lv_event_t* event);
    static void filterEventHandler(lv_event_t* event);
    static void scrollToBottomEventHandler(lv_event_t* event);
    static void autoScrollEventHandler(lv_event_t* event);

    lv_obj_t* log_container = nullptr;  // Scrollable container
    lv_obj_t* log_label = nullptr;       // Label instead of textarea (much faster)
    lv_obj_t* filter_bar = nullptr;
    lv_obj_t* btn_all = nullptr;
    lv_obj_t* btn_debug = nullptr;
    lv_obj_t* btn_info = nullptr;
    lv_obj_t* btn_warn = nullptr;
    lv_obj_t* btn_error = nullptr;
    lv_obj_t* btn_clear = nullptr;
    lv_obj_t* btn_scroll_bottom = nullptr;
    lv_obj_t* btn_auto_scroll = nullptr;

    lv_timer_t* log_timer = nullptr;
    uint32_t settings_listener_id = 0;

    // Cache for log view optimization
    size_t last_log_count = 0;
    String cached_log_text;

    // Filter state
    AppLogLevel current_filter = AppLogLevel::Debug;  // Default filter level
    bool show_all = true;                       // Track "All" separately for UI state
    bool auto_scroll_enabled = true;
};

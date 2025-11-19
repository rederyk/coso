#include "screens/system_log_screen.h"

#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "core/display_manager.h"
#include <esp_heap_caps.h>

SystemLogScreen::~SystemLogScreen() {
    if (log_timer) {
        lv_timer_del(log_timer);
        log_timer = nullptr;
    }
    detachSettingsListener();
}

void SystemLogScreen::build(lv_obj_t* parent) {
    if (!parent) {
        return;
    }

    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);

    // Top filter bar - compact buttons
    filter_bar = lv_obj_create(root);
    lv_obj_set_width(filter_bar, lv_pct(100));
    lv_obj_set_height(filter_bar, 36);
    lv_obj_set_layout(filter_bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(filter_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(filter_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(filter_bar, 4, 0);
    lv_obj_set_style_pad_gap(filter_bar, 3, 0);
    lv_obj_set_style_radius(filter_bar, 0, 0);
    lv_obj_set_style_border_width(filter_bar, 0, 0);

    // Helper lambda to create small buttons
    auto create_filter_btn = [this](const char* text, lv_event_cb_t callback, int level) -> lv_obj_t* {
        lv_obj_t* btn = lv_btn_create(filter_bar);
        lv_obj_set_size(btn, 40, 28);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_all(btn, 2, 0);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_center(label);

        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, (void*)(intptr_t)level);

        return btn;
    };

    // Filter buttons: All, D, I, W, E
    btn_all = create_filter_btn("All", filterEventHandler, -1);
    btn_debug = create_filter_btn("D", filterEventHandler, (int)LogLevel::Debug);
    btn_info = create_filter_btn("I", filterEventHandler, (int)LogLevel::Info);
    btn_warn = create_filter_btn("W", filterEventHandler, (int)LogLevel::Warn);
    btn_error = create_filter_btn("E", filterEventHandler, (int)LogLevel::Error);

    // Control buttons
    btn_clear = lv_btn_create(filter_bar);
    lv_obj_set_size(btn_clear, 28, 28);
    lv_obj_set_style_radius(btn_clear, 4, 0);
    lv_obj_t* clear_label = lv_label_create(btn_clear);
    lv_label_set_text(clear_label, LV_SYMBOL_TRASH);
    lv_obj_center(clear_label);
    lv_obj_add_event_cb(btn_clear, clearEventHandler, LV_EVENT_CLICKED, this);

    btn_scroll_bottom = lv_btn_create(filter_bar);
    lv_obj_set_size(btn_scroll_bottom, 28, 28);
    lv_obj_set_style_radius(btn_scroll_bottom, 4, 0);
    lv_obj_t* scroll_label = lv_label_create(btn_scroll_bottom);
    lv_label_set_text(scroll_label, LV_SYMBOL_DOWN);
    lv_obj_center(scroll_label);
    lv_obj_add_event_cb(btn_scroll_bottom, scrollToBottomEventHandler, LV_EVENT_CLICKED, this);

    btn_auto_scroll = lv_btn_create(filter_bar);
    lv_obj_set_size(btn_auto_scroll, 28, 28);
    lv_obj_set_style_radius(btn_auto_scroll, 4, 0);
    lv_obj_t* auto_label = lv_label_create(btn_auto_scroll);
    lv_label_set_text(auto_label, LV_SYMBOL_REFRESH);
    lv_obj_center(auto_label);
    lv_obj_add_event_cb(btn_auto_scroll, autoScrollEventHandler, LV_EVENT_CLICKED, this);

    // Log container with label - MUCH faster than textarea
    log_container = lv_obj_create(root);
    lv_obj_set_size(log_container, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(log_container, 1);
    lv_obj_set_style_pad_all(log_container, 4, 0);
    lv_obj_set_style_border_width(log_container, 1, 0);
    lv_obj_set_scrollbar_mode(log_container, LV_SCROLLBAR_MODE_AUTO);

    log_label = lv_label_create(log_container);
    lv_label_set_long_mode(log_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(log_label, lv_pct(100));
    lv_label_set_text(log_label, "");
    lv_obj_set_style_text_font(log_label, &lv_font_montserrat_14, 0);

    applyTheme(snapshot);
    updateFilterButtons();
    attachSettingsListener();
}

void SystemLogScreen::onShow() {
    Logger::getInstance().info(UI_SYMBOL_SYSLOG " System log screen shown");
    last_log_count = 0;  // Force initial refresh

    // Set placeholder text immediately - non blocking
    if (log_label) {
        lv_label_set_text(log_label, "Loading...");
    }

    // Defer the heavy log loading to a timer - this makes onShow() return immediately
    if (!log_timer) {
        log_timer = lv_timer_create([](lv_timer_t* timer) {
            if (!timer || !timer->user_data) return;
            auto* screen = static_cast<SystemLogScreen*>(timer->user_data);

            // First time: do the initial refresh
            screen->refreshLogView();

            // Then change to periodic updates every 2 seconds
            lv_timer_set_period(timer, 2000);
        }, 50, this);  // 50ms delay for first refresh
    }
}

void SystemLogScreen::onHide() {
    Logger::getInstance().info(UI_SYMBOL_SYSLOG " System log screen hidden");
    if (log_timer) {
        lv_timer_del(log_timer);
        log_timer = nullptr;
    }
}

void SystemLogScreen::refreshLogView() {
    if (!log_label) {
        return;
    }

    uint32_t t_start = millis();
    auto entries = Logger::getInstance().getBufferedLogs(current_filter);
    uint32_t t_get = millis();

    const size_t new_count = entries.size();

    // Check if logs have changed
    if (new_count == last_log_count && !cached_log_text.isEmpty()) {
        // No new logs, skip update
        return;
    }

    last_log_count = new_count;

    if (entries.empty()) {
        lv_label_set_text(log_label, "No logs.");
        return;
    }

    // Limit to last 40 entries for performance on small displays
    const size_t start_index = (new_count > 40) ? (new_count - 40) : 0;

    // Allocate buffer in PSRAM for better performance
    constexpr size_t BUFFER_SIZE = 12288;  // 12KB in PSRAM
    char* log_buffer = (char*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!log_buffer) {
        // Fallback to smaller DRAM buffer if PSRAM allocation fails
        log_buffer = (char*)malloc(4096);
        if (!log_buffer) {
            lv_label_set_text(log_label, "Memory error!");
            return;
        }
    }

    size_t pos = 0;

    if (start_index > 0) {
        pos += snprintf(log_buffer + pos, BUFFER_SIZE - pos,
                       "... %d earlier\n", (int)start_index);
    }

    // Build text directly in buffer for maximum speed
    for (size_t i = start_index; i < new_count && pos < BUFFER_SIZE - 2; ++i) {
        const char* line = entries[i].c_str();
        size_t line_len = entries[i].length();

        size_t available = BUFFER_SIZE - pos - 2;
        if (line_len > available) {
            line_len = available;
        }

        if (line_len > 0) {
            memcpy(log_buffer + pos, line, line_len);
            pos += line_len;
            log_buffer[pos++] = '\n';
        }
    }
    log_buffer[pos] = '\0';

    uint32_t t_build = millis();
    lv_label_set_text(log_label, log_buffer);
    uint32_t t_set = millis();

    // Free the buffer
    free(log_buffer);

    if (auto_scroll_enabled && log_container) {
        lv_obj_scroll_to_y(log_container, LV_COORD_MAX, LV_ANIM_OFF);
    }

    uint32_t t_end = millis();
    Serial.printf("[LogView] get=%lums build=%lums set=%lums scroll=%lums total=%lums entries=%d\n",
                  t_get - t_start, t_build - t_get, t_set - t_build,
                  t_end - t_set, t_end - t_start, (int)(new_count - start_index));
}

void SystemLogScreen::applyTheme(const SettingsSnapshot& snapshot) {
    const lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    const lv_color_t accent = lv_color_hex(snapshot.accentColor);
    const lv_color_t text_color = lv_color_hex(0xE0E0E0);
    const lv_color_t btn_bg = lv_color_darken(primary, 20);
    const lv_color_t selected_bg = accent;

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    }

    if (filter_bar) {
        lv_obj_set_style_bg_color(filter_bar, lv_color_darken(primary, 10), 0);
    }

    // Style all buttons
    lv_obj_t* buttons[] = {btn_all, btn_debug, btn_info, btn_warn, btn_error,
                           btn_clear, btn_scroll_bottom, btn_auto_scroll};
    for (auto* btn : buttons) {
        if (btn) {
            lv_obj_set_style_bg_color(btn, btn_bg, 0);
            lv_obj_set_style_text_color(btn, text_color, 0);
        }
    }

    if (log_container) {
        lv_obj_set_style_bg_color(log_container, lv_color_darken(primary, 30), 0);
        lv_obj_set_style_border_color(log_container, lv_color_darken(primary, 40), 0);
    }
    if (log_label) {
        lv_obj_set_style_text_color(log_label, text_color, 0);
    }

    updateFilterButtons();
}

void SystemLogScreen::updateFilterButtons() {
    const SettingsSnapshot& snapshot = SettingsManager::getInstance().getSnapshot();
    const lv_color_t accent = lv_color_hex(snapshot.accentColor);
    const lv_color_t btn_bg = lv_color_darken(lv_color_hex(snapshot.primaryColor), 20);

    // Update filter buttons to show active state
    auto set_btn_active = [accent, btn_bg](lv_obj_t* btn, bool active) {
        if (btn) {
            lv_obj_set_style_bg_color(btn, active ? accent : btn_bg, 0);
        }
    };

    set_btn_active(btn_all, current_filter == LogLevel::Debug);
    set_btn_active(btn_debug, current_filter == LogLevel::Debug && btn_all != nullptr);
    set_btn_active(btn_info, current_filter == LogLevel::Info);
    set_btn_active(btn_warn, current_filter == LogLevel::Warn);
    set_btn_active(btn_error, current_filter == LogLevel::Error);

    // Update auto-scroll button state
    if (btn_auto_scroll) {
        lv_obj_set_style_bg_color(btn_auto_scroll, auto_scroll_enabled ? accent : btn_bg, 0);
    }
}

void SystemLogScreen::attachSettingsListener() {
    if (settings_listener_id != 0) {
        return;
    }
    SettingsManager& manager = SettingsManager::getInstance();
    settings_listener_id = manager.addListener([this](SettingsManager::SettingKey key, const SettingsSnapshot& snapshot) {
        if (!root) {
            return;
        }
        switch (key) {
            case SettingsManager::SettingKey::ThemePrimaryColor:
            case SettingsManager::SettingKey::ThemeAccentColor:
                applyTheme(snapshot);
                break;
            default:
                break;
        }
    });
}

void SystemLogScreen::detachSettingsListener() {
    if (settings_listener_id == 0) {
        return;
    }
    SettingsManager::getInstance().removeListener(settings_listener_id);
    settings_listener_id = 0;
}

void SystemLogScreen::timerCallback(lv_timer_t* timer) {
    if (!timer || !timer->user_data) {
        return;
    }
    auto* screen = static_cast<SystemLogScreen*>(timer->user_data);
    screen->refreshLogView();
}

void SystemLogScreen::clearEventHandler(lv_event_t* event) {
    if (!event) {
        return;
    }
    auto* screen = static_cast<SystemLogScreen*>(lv_event_get_user_data(event));
    if (screen) {
        Logger::getInstance().clearBuffer();
        screen->last_log_count = 0;
        screen->cached_log_text = "";
        screen->refreshLogView();
    }
}

void SystemLogScreen::filterEventHandler(lv_event_t* event) {
    if (!event) {
        return;
    }
    auto* screen = static_cast<SystemLogScreen*>(lv_event_get_user_data(event));
    lv_obj_t* btn = lv_event_get_target(event);
    int level = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (screen) {
        if (level == -1) {
            screen->current_filter = LogLevel::Debug;  // Show all
        } else {
            screen->current_filter = static_cast<LogLevel>(level);
        }
        screen->last_log_count = 0;  // Force refresh
        screen->refreshLogView();
        screen->updateFilterButtons();
    }
}

void SystemLogScreen::scrollToBottomEventHandler(lv_event_t* event) {
    if (!event) {
        return;
    }
    auto* screen = static_cast<SystemLogScreen*>(lv_event_get_user_data(event));
    if (screen && screen->log_container) {
        lv_obj_scroll_to_y(screen->log_container, LV_COORD_MAX, LV_ANIM_ON);
    }
}

void SystemLogScreen::autoScrollEventHandler(lv_event_t* event) {
    if (!event) {
        return;
    }
    auto* screen = static_cast<SystemLogScreen*>(lv_event_get_user_data(event));
    if (screen) {
        screen->auto_scroll_enabled = !screen->auto_scroll_enabled;
        screen->updateFilterButtons();
    }
}

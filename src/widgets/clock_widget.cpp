#include "widgets/clock_widget.h"
#include "core/time_manager.h"

#include <Arduino.h>
#include <ctime>

ClockWidget::ClockWidget() : date_label(nullptr), time_label(nullptr), refresh_timer(nullptr) {}

ClockWidget::~ClockWidget() {
    if (refresh_timer) {
        lv_timer_del(refresh_timer);
        refresh_timer = nullptr;
    }
}

void ClockWidget::create(lv_obj_t* parent) {
    if (!parent) return;

    container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), 100);
    lv_obj_set_style_radius(container, 12, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x3b2b70), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 12, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Date label (top)
    date_label = lv_label_create(container);
    lv_label_set_text_static(date_label, "----/--/--");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xaaaaaa), 0);

    // Time label (bottom, larger)
    time_label = lv_label_create(container);
    lv_label_set_text_static(time_label, "--:--:--");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xffe66d), 0);

    refresh_timer = lv_timer_create(timerCallback, 1000, this);
    update();
}

void ClockWidget::update() {
    if (!date_label || !time_label) return;

    // Buffer statici per formattazione
    static char date_buffer[32];
    static char time_buffer[32];

    // Try to get real time from TimeManager
    auto& time_mgr = TimeManager::getInstance();
    if (time_mgr.isSynchronized()) {
        // Show real date and time
        struct tm local_time = time_mgr.getLocalTime();

        // Format date: "Mer 04 Dic 2024"
        const char* days[] = {"Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab"};
        const char* months[] = {"Gen", "Feb", "Mar", "Apr", "Mag", "Giu",
                                "Lug", "Ago", "Set", "Ott", "Nov", "Dic"};

        snprintf(date_buffer, sizeof(date_buffer), "%s %02d %s %04d",
                 days[local_time.tm_wday],
                 local_time.tm_mday,
                 months[local_time.tm_mon],
                 local_time.tm_year + 1900);

        // Format time: "15:30:45"
        snprintf(time_buffer, sizeof(time_buffer), "%02d:%02d:%02d",
                 local_time.tm_hour,
                 local_time.tm_min,
                 local_time.tm_sec);
    } else {
        // Fallback to uptime if time not synchronized
        snprintf(date_buffer, sizeof(date_buffer), "Not synced");

        uint32_t uptime_s = millis() / 1000;
        uint32_t hours = (uptime_s / 3600) % 24;
        uint32_t minutes = (uptime_s % 3600) / 60;
        uint32_t seconds = uptime_s % 60;
        snprintf(time_buffer, sizeof(time_buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    }

    const bool already_owned = lvgl_mutex_is_owned_by_current_task();
    bool lock_acquired = already_owned;
    if (!already_owned) {
        lock_acquired = lvgl_mutex_lock(pdMS_TO_TICKS(50));
    }

    if (lock_acquired) {
        lv_label_set_text(date_label, date_buffer);
        lv_label_set_text(time_label, time_buffer);
        if (!already_owned) {
            lvgl_mutex_unlock();
        }
    }
}

void ClockWidget::timerCallback(lv_timer_t* timer) {
    if (!timer || !timer->user_data) {
        return;
    }

    ClockWidget* self = static_cast<ClockWidget*>(timer->user_data);
    self->update();
}

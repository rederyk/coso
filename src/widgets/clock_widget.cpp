#include "widgets/clock_widget.h"

#include <Arduino.h>

ClockWidget::ClockWidget() : label(nullptr), refresh_timer(nullptr) {}

ClockWidget::~ClockWidget() {
    if (refresh_timer) {
        lv_timer_del(refresh_timer);
        refresh_timer = nullptr;
    }
}

void ClockWidget::create(lv_obj_t* parent) {
    if (!parent) return;

    container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), 80);
    lv_obj_set_style_radius(container, 12, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x3b2b70), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 8, 0);

    label = lv_label_create(container);
    lv_label_set_text_static(label, "--:--:--");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffe66d), 0);
    lv_obj_center(label);

    refresh_timer = lv_timer_create(timerCallback, 1000, this);
    update();
}

void ClockWidget::update() {
    if (!label) return;

    // Calcola i valori PRIMA di entrare nel mutex
    uint32_t uptime_s = millis() / 1000;
    uint32_t hours = (uptime_s / 3600) % 24;
    uint32_t minutes = (uptime_s % 3600) / 60;
    uint32_t seconds = uptime_s % 60;

    // Buffer statico per formattazione
    static char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);

    const bool already_owned = lvgl_mutex_is_owned_by_current_task();
    bool lock_acquired = already_owned;
    if (!already_owned) {
        lock_acquired = lvgl_mutex_lock(pdMS_TO_TICKS(50));
    }

    if (lock_acquired) {
        lv_label_set_text(label, buffer);
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

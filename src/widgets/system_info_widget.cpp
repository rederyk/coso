#include "widgets/system_info_widget.h"

#include <Arduino.h>

SystemInfoWidget::SystemInfoWidget() : heap_label(nullptr), uptime_label(nullptr), refresh_timer(nullptr) {}

SystemInfoWidget::~SystemInfoWidget() {
    if (refresh_timer) {
        lv_timer_del(refresh_timer);
        refresh_timer = nullptr;
    }
}

void SystemInfoWidget::create(lv_obj_t* parent) {
    if (!parent) return;

    container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), 100);
    lv_obj_set_style_radius(container, 12, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1d3557), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 10, 0);
    // Rimossi effetti ombra per risparmiare memoria
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    heap_label = lv_label_create(container);
    lv_label_set_text_static(heap_label, "Heap: -- KB");
    lv_obj_set_style_text_font(heap_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(heap_label, lv_color_hex(0xe0fbfc), 0);

    uptime_label = lv_label_create(container);
    lv_label_set_text_static(uptime_label, "Uptime: --:--:--");
    lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(uptime_label, lv_color_hex(0xaaefff), 0);

    refresh_timer = lv_timer_create(timerCallback, 2000, this);
    update();
}

void SystemInfoWidget::update() {
    if (!heap_label || !uptime_label) return;

    // Calcola i valori PRIMA di entrare nel mutex
    uint32_t free_heap = ESP.getFreeHeap();
    uint32_t uptime_s = millis() / 1000;
    uint32_t hours = uptime_s / 3600;
    uint32_t minutes = (uptime_s % 3600) / 60;
    uint32_t seconds = uptime_s % 60;

    // Buffer locali per formattazione
    static char heap_buf[32];
    static char uptime_buf[32];

    snprintf(heap_buf, sizeof(heap_buf), "Heap: %lu KB", free_heap / 1024UL);
    snprintf(uptime_buf, sizeof(uptime_buf), "Up: %02lu:%02lu:%02lu", hours, minutes, seconds);

    // Aggiorna UI con mutex - cattura RIFERIMENTI ai buffer statici
    if (lvgl_mutex_lock(pdMS_TO_TICKS(50))) {
        lv_label_set_text(heap_label, heap_buf);
        lv_label_set_text(uptime_label, uptime_buf);
        lvgl_mutex_unlock();
    }
}

void SystemInfoWidget::timerCallback(lv_timer_t* timer) {
    if (!timer || !timer->user_data) {
        return;
    }
    SystemInfoWidget* self = static_cast<SystemInfoWidget*>(timer->user_data);
    self->update();
}

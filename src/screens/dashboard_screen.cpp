#include "screens/dashboard_screen.h"

#include <lvgl.h>

void DashboardScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    root = lv_obj_create(parent);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x031427), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 10, 0);

    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text_static(header, "ESP32-S3 Dashboard");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0x5df4ff), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 5);

    lv_obj_t* layout = lv_obj_create(root);
    lv_obj_set_size(layout, lv_pct(95), lv_pct(75));
    lv_obj_align(layout, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(layout, lv_color_hex(0x0b2035), 0);
    lv_obj_set_style_border_width(layout, 0, 0);
    lv_obj_set_style_radius(layout, 12, 0);
    lv_obj_set_style_pad_all(layout, 10, 0);
    lv_obj_set_layout(layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(layout, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(layout, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    system_widget.create(layout);
    clock_widget.create(layout);
}

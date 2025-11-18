#include "screens/settings_screen.h"
#include <Arduino.h>

void SettingsScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    root = lv_obj_create(parent);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x0a0e27), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 15, 0);

    // Header con icona
    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text_static(header, "⚙️ Settings");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xf0f0f0), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 5);

    // Container centrale
    lv_obj_t* content = lv_obj_create(root);
    lv_obj_set_size(content, lv_pct(90), lv_pct(70));
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 15, 0);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    // Sezione Display
    lv_obj_t* display_section = lv_obj_create(content);
    lv_obj_set_size(display_section, lv_pct(100), 60);
    lv_obj_set_style_bg_color(display_section, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(display_section, 0, 0);
    lv_obj_set_style_radius(display_section, 10, 0);
    lv_obj_set_style_pad_all(display_section, 10, 0);

    brightness_label = lv_label_create(display_section);
    lv_label_set_text_static(brightness_label, "💡 Display: Auto");
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xe0e0e0), 0);
    lv_obj_align(brightness_label, LV_ALIGN_LEFT_MID, 5, 0);

    // Sezione WiFi
    lv_obj_t* wifi_section = lv_obj_create(content);
    lv_obj_set_size(wifi_section, lv_pct(100), 60);
    lv_obj_set_style_bg_color(wifi_section, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(wifi_section, 0, 0);
    lv_obj_set_style_radius(wifi_section, 10, 0);
    lv_obj_set_style_pad_all(wifi_section, 10, 0);

    lv_obj_t* wifi_label = lv_label_create(wifi_section);
    lv_label_set_text_static(wifi_label, "📶 WiFi: Disabled");
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xe0e0e0), 0);
    lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, 5, 0);

    // Versione
    version_label = lv_label_create(content);
    lv_label_set_text_static(version_label, "Version: 1.0.0");
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0x808080), 0);

    // Hint
    lv_obj_t* hint = lv_label_create(root);
    lv_label_set_text_static(hint, "👆 Swipe up for dock");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x606060), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
}

void SettingsScreen::onShow() {
    Serial.println("⚙️ Settings screen shown");
}

void SettingsScreen::onHide() {
    Serial.println("⚙️ Settings screen hidden");
}

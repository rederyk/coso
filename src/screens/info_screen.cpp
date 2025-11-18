#include "screens/info_screen.h"
#include <Arduino.h>
#include <esp_chip_info.h>

void InfoScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    root = lv_obj_create(parent);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 15, 0);

    // Header
    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text_static(header, "ℹ️ System Info");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xf0f0f0), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 5);

    // Container principale
    lv_obj_t* content = lv_obj_create(root);
    lv_obj_set_size(content, lv_pct(90), lv_pct(70));
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 15, 0);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    // Info chip
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    chip_label = lv_label_create(content);
    static char chip_buf[64];
    snprintf(chip_buf, sizeof(chip_buf), "🖥️ Chip: ESP32-S3 (Rev %d)", chip_info.revision);
    lv_label_set_text(chip_label, chip_buf);
    lv_obj_set_style_text_font(chip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(chip_label, lv_color_hex(0xe0e0e0), 0);

    // Cores
    lv_obj_t* cores_label = lv_label_create(content);
    static char cores_buf[64];
    snprintf(cores_buf, sizeof(cores_buf), "⚡ Cores: %d", chip_info.cores);
    lv_label_set_text(cores_label, cores_buf);
    lv_obj_set_style_text_font(cores_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cores_label, lv_color_hex(0xe0e0e0), 0);

    // Frequenza
    freq_label = lv_label_create(content);
    static char freq_buf[64];
    snprintf(freq_buf, sizeof(freq_buf), "📊 CPU Freq: %d MHz", ESP.getCpuFreqMHz());
    lv_label_set_text(freq_label, freq_buf);
    lv_obj_set_style_text_font(freq_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(freq_label, lv_color_hex(0xe0e0e0), 0);

    // Flash
    flash_label = lv_label_create(content);
    static char flash_buf[64];
    snprintf(flash_buf, sizeof(flash_buf), "💾 Flash: %d MB %s",
             ESP.getFlashChipSize() / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "(Embedded)" : "(External)");
    lv_label_set_text(flash_label, flash_buf);
    lv_obj_set_style_text_font(flash_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(flash_label, lv_color_hex(0xe0e0e0), 0);

    // SDK Version
    lv_obj_t* sdk_label = lv_label_create(content);
    static char sdk_buf[64];
    snprintf(sdk_buf, sizeof(sdk_buf), "🔧 SDK: %s", ESP.getSdkVersion());
    lv_label_set_text(sdk_label, sdk_buf);
    lv_obj_set_style_text_font(sdk_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sdk_label, lv_color_hex(0xb0b0b0), 0);

    // Hint
    lv_obj_t* hint = lv_label_create(root);
    lv_label_set_text_static(hint, "👆 Swipe up for dock");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x606060), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
}

void InfoScreen::onShow() {
    Serial.println("ℹ️ Info screen shown");
}

void InfoScreen::onHide() {
    Serial.println("ℹ️ Info screen hidden");
}

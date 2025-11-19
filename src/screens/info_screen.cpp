#include "screens/info_screen.h"
#include <Arduino.h>
#include <esp_chip_info.h>
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "core/settings_manager.h"

void InfoScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    root = lv_obj_create(parent);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_pad_all(root, 15, 0);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(root, 12, 0);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_AUTO);

    // Header
    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text_static(header, UI_SYMBOL_INFO " System Info");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xf0f0f0), 0);

    // Hardware info container
    lv_obj_t* content = lv_obj_create(root);
    lv_obj_set_size(content, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 15, 0);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 8, 0);

    // Info chip
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    chip_label = lv_label_create(content);
    static char chip_buf[64];
    snprintf(chip_buf, sizeof(chip_buf), UI_SYMBOL_CHIP " Chip: ESP32-S3 (Rev %d)", chip_info.revision);
    lv_label_set_text(chip_label, chip_buf);
    lv_obj_set_style_text_font(chip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(chip_label, lv_color_hex(0xe0e0e0), 0);

    // Cores
    lv_obj_t* cores_label = lv_label_create(content);
    static char cores_buf[64];
    snprintf(cores_buf, sizeof(cores_buf), UI_SYMBOL_POWER " Cores: %d", chip_info.cores);
    lv_label_set_text(cores_label, cores_buf);
    lv_obj_set_style_text_font(cores_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cores_label, lv_color_hex(0xe0e0e0), 0);

    // Frequenza
    freq_label = lv_label_create(content);
    static char freq_buf[64];
    snprintf(freq_buf, sizeof(freq_buf), UI_SYMBOL_CHART " CPU Freq: %d MHz", ESP.getCpuFreqMHz());
    lv_label_set_text(freq_label, freq_buf);
    lv_obj_set_style_text_font(freq_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(freq_label, lv_color_hex(0xe0e0e0), 0);

    // Flash
    flash_label = lv_label_create(content);
    static char flash_buf[64];
    snprintf(flash_buf, sizeof(flash_buf), UI_SYMBOL_STORAGE " Flash: %d MB %s",
             ESP.getFlashChipSize() / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "(Embedded)" : "(External)");
    lv_label_set_text(flash_label, flash_buf);
    lv_obj_set_style_text_font(flash_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(flash_label, lv_color_hex(0xe0e0e0), 0);

    // SDK Version
    lv_obj_t* sdk_label = lv_label_create(content);
    static char sdk_buf[64];
    snprintf(sdk_buf, sizeof(sdk_buf), UI_SYMBOL_TOOL " SDK: %s", ESP.getSdkVersion());
    lv_label_set_text(sdk_label, sdk_buf);
    lv_obj_set_style_text_font(sdk_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sdk_label, lv_color_hex(0xb0b0b0), 0);

    // System Controls card - Separate from hardware info
    lv_obj_t* controls_container = lv_obj_create(root);
    lv_obj_set_size(controls_container, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(controls_container, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(controls_container, 0, 0);
    lv_obj_set_style_radius(controls_container, 15, 0);
    lv_obj_set_style_pad_all(controls_container, 12, 0);
    lv_obj_set_layout(controls_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(controls_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(controls_container, 10, 0);

    // Title for controls
    lv_obj_t* controls_title = lv_label_create(controls_container);
    lv_label_set_text(controls_title, "⚙️  Controlli Sistema");
    lv_obj_set_style_text_font(controls_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(controls_title, lv_color_hex(0xf0f0f0), 0);

    // Reset button
    lv_obj_t* reset_btn = lv_btn_create(controls_container);
    lv_obj_set_width(reset_btn, lv_pct(100));
    lv_obj_set_height(reset_btn, 45);
    lv_obj_add_event_cb(reset_btn, handleResetButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xff6600), 0);
    lv_obj_t* reset_btn_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_btn_label, LV_SYMBOL_REFRESH " Reset Impostazioni");
    lv_obj_center(reset_btn_label);
    lv_obj_set_style_text_font(reset_btn_label, &lv_font_montserrat_14, 0);

    // Reboot button
    lv_obj_t* reboot_btn = lv_btn_create(controls_container);
    lv_obj_set_width(reboot_btn, lv_pct(100));
    lv_obj_set_height(reboot_btn, 45);
    lv_obj_add_event_cb(reboot_btn, handleRebootButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(reboot_btn, lv_color_hex(0xff0000), 0);
    lv_obj_t* reboot_btn_label = lv_label_create(reboot_btn);
    lv_label_set_text(reboot_btn_label, LV_SYMBOL_POWER " Riavvia Sistema");
    lv_obj_center(reboot_btn_label);
    lv_obj_set_style_text_font(reboot_btn_label, &lv_font_montserrat_14, 0);

    // Hint
    lv_obj_t* hint = lv_label_create(root);
    lv_label_set_text_static(hint, LV_SYMBOL_UP " Swipe up for dock");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x606060), 0);
}

void InfoScreen::onShow() {
    Logger::getInstance().info(UI_SYMBOL_INFO " Info screen shown");
}

void InfoScreen::onHide() {
    Logger::getInstance().info(UI_SYMBOL_INFO " Info screen hidden");
}

void InfoScreen::handleResetButton(lv_event_t* e) {
    auto* screen = static_cast<InfoScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    // Create confirmation dialog
    static const char* reset_btns[] = {"Annulla", "Reset", ""};
    lv_obj_t* mbox = lv_msgbox_create(NULL, "Conferma Reset",
        "Ripristinare le impostazioni ai valori predefiniti?\n\nQuesta operazione è irreversibile.",
        reset_btns, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, confirmReset, LV_EVENT_VALUE_CHANGED, screen);
}

void InfoScreen::handleRebootButton(lv_event_t* e) {
    auto* screen = static_cast<InfoScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    // Create confirmation dialog
    static const char* reboot_btns[] = {"Annulla", "Riavvia", ""};
    lv_obj_t* mbox = lv_msgbox_create(NULL, "Conferma Riavvio",
        "Riavviare il sistema?\n\nTutte le impostazioni saranno salvate.",
        reboot_btns, true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, confirmReboot, LV_EVENT_VALUE_CHANGED, screen);
}

void InfoScreen::confirmReset(lv_event_t* e) {
    lv_obj_t* mbox = lv_event_get_current_target(e);
    uint16_t btn_id = lv_msgbox_get_active_btn(mbox);

    if (btn_id == 1) { // "Reset" button
        Logger::getInstance().warn("[System] Resetting to defaults...");
        SettingsManager::getInstance().reset();
        Logger::getInstance().info("[System] Reset complete");
    }

    lv_msgbox_close(mbox);
}

void InfoScreen::confirmReboot(lv_event_t* e) {
    lv_obj_t* mbox = lv_event_get_current_target(e);
    uint16_t btn_id = lv_msgbox_get_active_btn(mbox);

    if (btn_id == 1) { // "Riavvia" button
        Logger::getInstance().warn("[System] Rebooting...");
        lv_msgbox_close(mbox);
        delay(500);
        ESP.restart();
    }

    lv_msgbox_close(mbox);
}

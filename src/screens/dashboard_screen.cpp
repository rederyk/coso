#include "screens/dashboard_screen.h"
#include "core/wifi_manager.h"
#include "core/ble_hid_manager.h"
#include "drivers/sd_card_driver.h"
#include "ui/ui_symbols.h"
#include <lvgl.h>
#include <WiFi.h>

DashboardScreen::~DashboardScreen() {
    if (status_timer) {
        lv_timer_del(status_timer);
        status_timer = nullptr;
    }
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void DashboardScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    root = lv_obj_create(parent);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 12, 0);

    // Status bar (WiFi, BLE, SD icons) - Top center
    status_bar = lv_obj_create(root);
    lv_obj_set_size(status_bar, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(status_bar, 5, 0);
    lv_obj_set_layout(status_bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_bar, 12, 0);

    wifi_status_label = lv_label_create(status_bar);
    lv_label_set_text(wifi_status_label, UI_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_16, 0);

    ble_status_label = lv_label_create(status_bar);
    lv_label_set_text(ble_status_label, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(ble_status_label, &lv_font_montserrat_16, 0);

    sd_status_label = lv_label_create(status_bar);
    lv_label_set_text(sd_status_label, UI_SYMBOL_STORAGE);
    lv_obj_set_style_text_font(sd_status_label, &lv_font_montserrat_16, 0);

    // Title label - Below status bar
    header_label = lv_label_create(root);
    lv_label_set_text_static(header_label, "ESP32-S3 Dashboard");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_20, 0);
    lv_obj_align_to(header_label, status_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    layout = lv_obj_create(root);
    lv_obj_set_size(layout, lv_pct(95), lv_pct(75));
    lv_obj_align(layout, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_border_width(layout, 0, 0);
    lv_obj_set_style_pad_all(layout, 10, 0);
    lv_obj_set_layout(layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_align(layout, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    system_widget.create(layout);
    clock_widget.create(layout);
    weather_widget.create(layout);

    applyTheme(snapshot);
    updateStatusIcons();

    // Create timer for status updates (every 3 seconds)
    status_timer = lv_timer_create(statusUpdateTimer, 3000, this);

    if (settings_listener_id == 0) {
        settings_listener_id = settings.addListener([this](SettingsManager::SettingKey key, const SettingsSnapshot& snap) {
            switch (key) {
                case SettingsManager::SettingKey::Theme:
                case SettingsManager::SettingKey::ThemePrimaryColor:
                case SettingsManager::SettingKey::ThemeAccentColor:
                case SettingsManager::SettingKey::ThemeBorderRadius:
                case SettingsManager::SettingKey::LayoutOrientation:
                    applyTheme(snap);
                    break;
                default:
                    break;
            }
        });
    }
}

void DashboardScreen::onShow() {
    applyTheme(SettingsManager::getInstance().getSnapshot());
    updateStatusIcons();
}

void DashboardScreen::onHide() {
    // nothing for now
}

void DashboardScreen::updateStatusIcons() {
    if (!wifi_status_label || !ble_status_label || !sd_status_label) {
        return;
    }

    const SettingsSnapshot& snapshot = SettingsManager::getInstance().getSnapshot();
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t inactive_color = lv_color_hex(0x606060);

    // WiFi status
    bool wifi_connected = WiFi.status() == WL_CONNECTED;
    lv_obj_set_style_text_color(wifi_status_label, wifi_connected ? accent : inactive_color, 0);
    lv_obj_set_style_opa(wifi_status_label, wifi_connected ? LV_OPA_COVER : LV_OPA_50, 0);

    // BLE status - check if BLE is initialized
    BleHidManager& ble = BleHidManager::getInstance();
    bool ble_active = ble.isEnabled() && ble.isInitialized();
    lv_obj_set_style_text_color(ble_status_label, ble_active ? accent : inactive_color, 0);
    lv_obj_set_style_opa(ble_status_label, ble_active ? LV_OPA_COVER : LV_OPA_50, 0);

    // SD card status
    SdCardDriver& sd = SdCardDriver::getInstance();
    bool sd_mounted = sd.isMounted();
    lv_obj_set_style_text_color(sd_status_label, sd_mounted ? accent : inactive_color, 0);
    lv_obj_set_style_opa(sd_status_label, sd_mounted ? LV_OPA_COVER : LV_OPA_50, 0);
}

void DashboardScreen::statusUpdateTimer(lv_timer_t* timer) {
    auto* screen = static_cast<DashboardScreen*>(timer->user_data);
    if (screen) {
        screen->updateStatusIcons();
    }
}

void DashboardScreen::applyTheme(const SettingsSnapshot& snapshot) {
    if (!root) {
        return;
    }
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t layout_bg = lv_color_mix(accent, primary, LV_OPA_40);

    lv_obj_set_style_bg_color(root, primary, 0);
    if (header_label) {
        lv_obj_set_style_text_color(header_label, accent, 0);
    }
    if (layout) {
        lv_obj_set_style_bg_color(layout, layout_bg, 0);
        lv_obj_set_style_radius(layout, snapshot.borderRadius, 0);
        lv_obj_set_flex_flow(layout, snapshot.landscapeLayout ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(layout, snapshot.landscapeLayout ? 12 : 8, 0);
        lv_obj_set_style_pad_column(layout, snapshot.landscapeLayout ? 12 : 0, 0);
    }

    // Update status icon colors when theme changes
    updateStatusIcons();
}

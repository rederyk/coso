#include "screens/dashboard_screen.h"

#include <lvgl.h>

DashboardScreen::~DashboardScreen() {
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

    header_label = lv_label_create(root);
    lv_label_set_text_static(header_label, "ESP32-S3 Dashboard");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_20, 0);
    lv_obj_align(header_label, LV_ALIGN_TOP_MID, 0, 5);

    layout = lv_obj_create(root);
    lv_obj_set_size(layout, lv_pct(95), lv_pct(75));
    lv_obj_align(layout, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_border_width(layout, 0, 0);
    lv_obj_set_style_pad_all(layout, 10, 0);
    lv_obj_set_layout(layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_align(layout, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    system_widget.create(layout);
    clock_widget.create(layout);

    applyTheme(snapshot);

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
}

void DashboardScreen::onHide() {
    // nothing for now
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
}

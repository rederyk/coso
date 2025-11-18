#include "screens/settings_screen.h"
#include <Arduino.h>

namespace {
struct ThemeOption {
    const char* id;
    const char* label;
};

constexpr ThemeOption THEME_OPTIONS[] = {
    {"dark", "Dark"},
    {"light", "Light"},
    {"auto", "Auto"}
};

lv_obj_t* create_card(lv_obj_t* parent, const char* title, const char* subtitle = nullptr) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_style_bg_color(card, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    lv_obj_t* title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xf0f0f0), 0);

    if (subtitle) {
        lv_obj_t* subtitle_lbl = lv_label_create(card);
        lv_label_set_text(subtitle_lbl, subtitle);
        lv_obj_set_style_text_font(subtitle_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(subtitle_lbl, lv_color_hex(0xa0a0a0), 0);
    }

    return card;
}
} // namespace

SettingsScreen::~SettingsScreen() {
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void SettingsScreen::build(lv_obj_t* parent) {
    if (!parent) {
        return;
    }

    SettingsManager& manager = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = manager.getSnapshot();

    root = lv_obj_create(parent);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x0a0e27), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 16, 0);

    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text(header, "⚙️ Settings");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xffffff), 0);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 4, 0);

    lv_obj_t* content = lv_obj_create(root);
    lv_obj_set_size(content, lv_pct(100), lv_pct(80));
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 14, 0);

    // WiFi credentials
    lv_obj_t* wifi_card = create_card(content, "📶 WiFi", "Configura SSID e password della rete");

    wifi_ssid_input = lv_textarea_create(wifi_card);
    lv_textarea_set_one_line(wifi_ssid_input, true);
    lv_textarea_set_max_length(wifi_ssid_input, 63);
    lv_textarea_set_placeholder_text(wifi_ssid_input, "SSID");
    lv_obj_set_width(wifi_ssid_input, lv_pct(100));
    lv_obj_add_event_cb(wifi_ssid_input, handleTextInput, LV_EVENT_VALUE_CHANGED, this);

    wifi_pass_input = lv_textarea_create(wifi_card);
    lv_textarea_set_one_line(wifi_pass_input, true);
    lv_textarea_set_password_mode(wifi_pass_input, true);
    lv_textarea_set_max_length(wifi_pass_input, 63);
    lv_textarea_set_placeholder_text(wifi_pass_input, "Password");
    lv_obj_set_width(wifi_pass_input, lv_pct(100));
    lv_obj_add_event_cb(wifi_pass_input, handleTextInput, LV_EVENT_VALUE_CHANGED, this);

    // Brightness
    lv_obj_t* display_card = create_card(content, "💡 Display", "Regola la luminosità del backlight (0-100%)");
    brightness_slider = lv_slider_create(display_card);
    lv_obj_set_width(brightness_slider, lv_pct(100));
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_obj_add_event_cb(brightness_slider, handleBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

    brightness_value_label = lv_label_create(display_card);
    lv_obj_set_style_text_font(brightness_value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(brightness_value_label, lv_color_hex(0xe0e0e0), 0);

    // Theme selector
    lv_obj_t* theme_card = create_card(content, "🎨 Tema", "Seleziona la palette preferita");
    theme_dropdown = lv_dropdown_create(theme_card);
    lv_obj_set_width(theme_dropdown, lv_pct(100));
    lv_dropdown_set_options(theme_dropdown, "Dark\nLight\nAuto");
    lv_obj_add_event_cb(theme_dropdown, handleThemeChanged, LV_EVENT_VALUE_CHANGED, this);

    // Version
    lv_obj_t* info_card = create_card(content, "ℹ️ Sistema", "Versione firmware e suggerimenti UI");
    version_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0xc0c0c0), 0);

    hint_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x909090), 0);
    lv_label_set_text(hint_label, "Dati salvati automaticamente su NVS.");

    applySnapshot(snapshot);

    if (settings_listener_id == 0) {
        settings_listener_id = manager.addListener([this](SettingsManager::SettingKey, const SettingsSnapshot& snap) {
            if (!root) {
                return;
            }
            applySnapshot(snap);
        });
    }
}

void SettingsScreen::onShow() {
    Serial.println("⚙️ Settings screen shown");
    applySnapshot(SettingsManager::getInstance().getSnapshot());
}

void SettingsScreen::onHide() {
    Serial.println("⚙️ Settings screen hidden");
}

void SettingsScreen::applySnapshot(const SettingsSnapshot& snapshot) {
    updating_from_manager = true;

    if (wifi_ssid_input) {
        lv_textarea_set_text(wifi_ssid_input, snapshot.wifiSsid.c_str());
    }
    if (wifi_pass_input) {
        lv_textarea_set_text(wifi_pass_input, snapshot.wifiPassword.c_str());
    }
    if (brightness_slider) {
        lv_slider_set_value(brightness_slider, snapshot.brightness, LV_ANIM_OFF);
        updateBrightnessLabel(snapshot.brightness);
    }
    if (theme_dropdown) {
        const size_t index = themeIndexFromId(snapshot.theme);
        lv_dropdown_set_selected(theme_dropdown, index);
    }
    if (version_label) {
        const char* version = snapshot.version.empty() ? "unknown" : snapshot.version.c_str();
        lv_label_set_text_fmt(version_label, "Versione firmware: %s", version);
    }

    updating_from_manager = false;
}

void SettingsScreen::updateBrightnessLabel(uint8_t value) {
    if (!brightness_value_label) {
        return;
    }
    lv_label_set_text_fmt(brightness_value_label, "%u %%", value);
}

size_t SettingsScreen::themeIndexFromId(const std::string& theme_id) const {
    for (size_t i = 0; i < sizeof(THEME_OPTIONS) / sizeof(THEME_OPTIONS[0]); ++i) {
        if (theme_id == THEME_OPTIONS[i].id) {
            return i;
        }
    }
    return 0;
}

const char* SettingsScreen::themeIdFromIndex(size_t index) const {
    if (index >= sizeof(THEME_OPTIONS) / sizeof(THEME_OPTIONS[0])) {
        return THEME_OPTIONS[0].id;
    }
    return THEME_OPTIONS[index].id;
}

void SettingsScreen::handleTextInput(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) {
        return;
    }
    lv_obj_t* target = lv_event_get_target(e);
    const char* text = lv_textarea_get_text(target);
    SettingsManager& manager = SettingsManager::getInstance();

    if (target == screen->wifi_ssid_input) {
        manager.setWifiSsid(text ? text : "");
    } else if (target == screen->wifi_pass_input) {
        manager.setWifiPassword(text ? text : "");
    }
}

void SettingsScreen::handleBrightnessChanged(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) {
        return;
    }
    lv_obj_t* slider = lv_event_get_target(e);
    uint16_t value = lv_slider_get_value(slider);
    screen->updateBrightnessLabel(value);
    SettingsManager::getInstance().setBrightness(static_cast<uint8_t>(value));
}

void SettingsScreen::handleThemeChanged(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) {
        return;
    }
    uint16_t selected = lv_dropdown_get_selected(screen->theme_dropdown);
    const char* theme_id = screen->themeIdFromIndex(selected);
    SettingsManager::getInstance().setTheme(theme_id ? theme_id : "dark");
}

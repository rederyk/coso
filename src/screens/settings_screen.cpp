#include "screens/settings_screen.h"
#include <Arduino.h>
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/color_utils.h"
#include "core/app_manager.h"
#include "drivers/rgb_led_driver.h"
#include "core/operating_modes.h"


namespace {
lv_obj_t* create_card(lv_obj_t* parent, const char* title, const char* subtitle = nullptr, lv_color_t bg_color = lv_color_hex(0x0f3460)) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, bg_color, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    // Inverte automaticamente il colore del testo in base allo sfondo
    lv_color_t text_color = ColorUtils::invertColor(bg_color);
    lv_color_t subtitle_color = ColorUtils::getMutedTextColor(bg_color);

    lv_obj_t* title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_lbl, text_color, 0);

    if (subtitle) {
        lv_obj_t* subtitle_lbl = lv_label_create(card);
        lv_label_set_text(subtitle_lbl, subtitle);
        lv_obj_set_style_text_font(subtitle_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(subtitle_lbl, subtitle_color, 0);
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
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_set_style_pad_row(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_outline_width(root, 0, 0);

    header_label = lv_label_create(root);
    lv_label_set_text(header_label, UI_SYMBOL_SETTINGS " Settings");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_24, 0);
    lv_obj_set_width(header_label, lv_pct(100));

    content_container = lv_obj_create(root);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_width(content_container, lv_pct(100));
    lv_obj_set_height(content_container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content_container, 4, 0);
    lv_obj_set_layout(content_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content_container, 12, 0);
    lv_obj_clear_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);

    // Connectivity Settings - Compact version without title
    connectivity_card = lv_obj_create(content_container);
    lv_obj_set_width(connectivity_card, lv_pct(100));
    lv_obj_set_height(connectivity_card, LV_SIZE_CONTENT);
    lv_obj_clear_flag(connectivity_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(connectivity_card, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_border_width(connectivity_card, 0, 0);
    lv_obj_set_style_radius(connectivity_card, 12, 0);
    lv_obj_set_style_pad_all(connectivity_card, 10, 0);
    lv_obj_set_layout(connectivity_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(connectivity_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(connectivity_card, 8, 0);

    lv_obj_t* wifi_settings_btn = lv_btn_create(connectivity_card);
    lv_obj_set_width(wifi_settings_btn, lv_pct(100));
    lv_obj_set_height(wifi_settings_btn, 50);
    lv_obj_add_event_cb(wifi_settings_btn, handleWifiSettingsButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(wifi_settings_btn, lv_color_hex(0x0066cc), 0);
    lv_obj_t* wifi_btn_label = lv_label_create(wifi_settings_btn);
    lv_label_set_text(wifi_btn_label, UI_SYMBOL_WIFI " Impostazioni WiFi");
    lv_obj_center(wifi_btn_label);
    lv_obj_set_style_text_font(wifi_btn_label, &lv_font_montserrat_16, 0);

    lv_obj_t* ble_settings_btn = lv_btn_create(connectivity_card);
    lv_obj_set_width(ble_settings_btn, lv_pct(100));
    lv_obj_set_height(ble_settings_btn, 50);
    lv_obj_add_event_cb(ble_settings_btn, handleBleSettingsButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(ble_settings_btn, lv_color_hex(0x0066cc), 0);
    lv_obj_t* ble_btn_label = lv_label_create(ble_settings_btn);
    lv_label_set_text(ble_btn_label, LV_SYMBOL_BLUETOOTH " Impostazioni BLE");
    lv_obj_center(ble_btn_label);
    lv_obj_set_style_text_font(ble_btn_label, &lv_font_montserrat_16, 0);

    lv_obj_t* voice_settings_btn = lv_btn_create(connectivity_card);
    lv_obj_set_width(voice_settings_btn, lv_pct(100));
    lv_obj_set_height(voice_settings_btn, 50);
    lv_obj_add_event_cb(voice_settings_btn, handleVoiceAssistantSettingsButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(voice_settings_btn, lv_color_hex(0x0066cc), 0);
    lv_obj_t* voice_btn_label = lv_label_create(voice_settings_btn);
    lv_label_set_text(voice_btn_label, LV_SYMBOL_AUDIO " Assistant Vocale");
    lv_obj_center(voice_btn_label);
    lv_obj_set_style_text_font(voice_btn_label, &lv_font_montserrat_16, 0);

    // Brightness
    display_card = create_card(content_container, UI_SYMBOL_BRIGHTNESS " Display", "backlight (1-100%)");
    brightness_slider = lv_slider_create(display_card);
    lv_obj_set_width(brightness_slider, lv_pct(100));
    lv_slider_set_range(brightness_slider, 1, 100);
    lv_obj_add_event_cb(brightness_slider, handleBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

    brightness_value_label = lv_label_create(display_card);
    lv_obj_set_style_text_font(brightness_value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(brightness_value_label, lv_color_hex(0xe0e0e0), 0);

    // RGB LED Brightness
    led_card = create_card(content_container, "💡 RGB LED", "luminosità (0-100%)");

    // LED Settings button
    lv_obj_t* led_settings_btn = lv_btn_create(led_card);
    lv_obj_set_width(led_settings_btn, lv_pct(100));
    lv_obj_set_height(led_settings_btn, 40);
    lv_obj_add_event_cb(led_settings_btn, handleLedSettingsButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(led_settings_btn, lv_color_hex(0x0066cc), 0);
    lv_obj_t* led_settings_btn_label = lv_label_create(led_settings_btn);
    lv_label_set_text(led_settings_btn_label, UI_SYMBOL_LED " LED Avanzate");
    lv_obj_center(led_settings_btn_label);
    lv_obj_set_style_text_font(led_settings_btn_label, &lv_font_montserrat_14, 0);

    led_brightness_slider = lv_slider_create(led_card);
    lv_obj_set_width(led_brightness_slider, lv_pct(100));
    lv_slider_set_range(led_brightness_slider, 0, 100);
    lv_obj_add_event_cb(led_brightness_slider, handleLedBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

    led_brightness_value_label = lv_label_create(led_card);
    lv_obj_set_style_text_font(led_brightness_value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(led_brightness_value_label, lv_color_hex(0xe0e0e0), 0);

    // Operating Mode
    operating_card = create_card(content_container, "⚙️ Operating Mode", "Select UI/Web configuration");
    mode_dropdown = lv_dropdown_create(operating_card);
    lv_dropdown_set_options(mode_dropdown, "Full (UI + Web)\nUI Only\nWeb Only");
    lv_obj_set_width(mode_dropdown, lv_pct(100));
    lv_obj_add_event_cb(mode_dropdown, handleModeChanged, LV_EVENT_VALUE_CHANGED, this);

 
    // Version Info
    info_card = create_card(content_container, UI_SYMBOL_INFO " Info Sistema", "Versione firmware e suggerimenti");

    version_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0xc0c0c0), 0);

    hint_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x909090), 0);
    lv_label_set_text(hint_label, "Dati salvati automaticamente su LittleFS.");

    // Developer button (at bottom)
    lv_obj_t* developer_btn = lv_btn_create(content_container);
    lv_obj_set_width(developer_btn, lv_pct(100));
    lv_obj_set_height(developer_btn, 50);
    lv_obj_add_event_cb(developer_btn, handleDeveloperButton, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(developer_btn, lv_color_hex(0x404040), 0);
    lv_obj_t* developer_btn_label = lv_label_create(developer_btn);
    lv_label_set_text(developer_btn_label, UI_SYMBOL_SETTINGS " Developer");
    lv_obj_center(developer_btn_label);
    lv_obj_set_style_text_font(developer_btn_label, &lv_font_montserrat_16, 0);

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
    Logger::getInstance().info(UI_SYMBOL_SETTINGS " Settings screen shown");
    applySnapshot(SettingsManager::getInstance().getSnapshot());
}

void SettingsScreen::onHide() {
    Logger::getInstance().info(UI_SYMBOL_SETTINGS " Settings screen hidden");
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
    if (led_brightness_slider) {
        lv_slider_set_value(led_brightness_slider, snapshot.ledBrightness, LV_ANIM_OFF);
        updateLedBrightnessLabel(snapshot.ledBrightness);
    }
    if (version_label) {
        const char* version = snapshot.version.empty() ? "unknown" : snapshot.version.c_str();
        lv_label_set_text_fmt(version_label, "Versione firmware: %s", version);
    }

    // Update operating mode dropdown
    if (mode_dropdown) {
        int sel = 0;
        switch (snapshot.operatingMode) {
            case OPERATING_MODE_FULL: sel = 0; break;
            case OPERATING_MODE_UI_ONLY: sel = 1; break;
            case OPERATING_MODE_WEB_ONLY: sel = 2; break;
            default: sel = 0; break;
        }
        lv_dropdown_set_selected(mode_dropdown, sel);
    }

    applyThemeStyles(snapshot);
    updating_from_manager = false;
}


void SettingsScreen::applyThemeStyles(const SettingsSnapshot& snapshot) {
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t hint_color = lv_color_mix(accent, lv_color_hex(0xffffff), LV_OPA_40);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
    }
    if (header_label) {
        lv_obj_set_style_text_color(header_label, accent, 0);
    }
    if (content_container) {
        lv_obj_set_flex_flow(content_container, snapshot.landscapeLayout ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_column(content_container, snapshot.landscapeLayout ? 12 : 0, 0);
        lv_obj_set_style_pad_row(content_container, snapshot.landscapeLayout ? 12 : 14, 0);
    }
    if (hint_label) {
        lv_obj_set_style_text_color(hint_label, hint_color, 0);
    }

    styleCard(connectivity_card, false, snapshot);
    styleCard(display_card, true, snapshot);
    styleCard(led_card, true, snapshot);

    styleCard(operating_card, true, snapshot);

    styleCard(info_card, false, snapshot);

}

void SettingsScreen::styleCard(lv_obj_t* card, bool allow_half_width, const SettingsSnapshot& snapshot) {
    if (!card) {
        return;
    }
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t card_bg = lv_color_mix(accent, primary, LV_OPA_30);

    lv_obj_set_style_bg_color(card, card_bg, 0);
    lv_obj_set_style_radius(card, snapshot.borderRadius, 0);
    lv_obj_set_style_border_width(card, 0, 0);

    // Inverte automaticamente il colore del testo in base al colore della card
    lv_color_t text_color = ColorUtils::invertColor(card_bg);
    lv_color_t subtitle_color = ColorUtils::getMutedTextColor(card_bg);

    // Aggiorna tutti i label figli della card
    uint32_t child_count = lv_obj_get_child_cnt(card);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(card, i);
        if (child && lv_obj_check_type(child, &lv_label_class)) {
            // Usa il font per determinare se è un titolo o un sottotitolo
            const lv_font_t* font = lv_obj_get_style_text_font(child, 0);
            if (font == &lv_font_montserrat_20) {
                // Titolo principale
                lv_obj_set_style_text_color(child, text_color, 0);
            } else if (font == &lv_font_montserrat_14) {
                // Sottotitolo
                lv_obj_set_style_text_color(child, subtitle_color, 0);
            } else {
                // Default: colore standard
                lv_obj_set_style_text_color(child, text_color, 0);
            }
        }
    }

    if (snapshot.landscapeLayout && allow_half_width) {
        lv_obj_set_width(card, lv_pct(48));
    } else {
        lv_obj_set_width(card, lv_pct(100));
    }
}

void SettingsScreen::updateBrightnessLabel(uint8_t value) {
    if (!brightness_value_label) {
        return;
    }
    lv_label_set_text_fmt(brightness_value_label, "%u %%", value);
}

void SettingsScreen::updateLedBrightnessLabel(uint8_t value) {
    if (!led_brightness_value_label) {
        return;
    }
    lv_label_set_text_fmt(led_brightness_value_label, "%u %%", value);
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

void SettingsScreen::handleLedBrightnessChanged(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) {
        return;
    }
    lv_obj_t* slider = lv_event_get_target(e);
    uint16_t value = lv_slider_get_value(slider);
    screen->updateLedBrightnessLabel(value);
    SettingsManager::getInstance().setLedBrightness(static_cast<uint8_t>(value));
}

void SettingsScreen::handleWifiSettingsButton(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    Logger::getInstance().info("[Settings] Launching WiFi settings...");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("WiFiSettings");
    }
}

void SettingsScreen::handleBleSettingsButton(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    Logger::getInstance().info("[Settings] Launching BLE settings...");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("BleSettings");
    }
}

void SettingsScreen::handleLedSettingsButton(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    Logger::getInstance().info("[Settings] Launching LED settings...");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("LedSettings");
    }
}

void SettingsScreen::handleDeveloperButton(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    Logger::getInstance().info("[Settings] Launching Developer screen...");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("Developer");
    }
}

void SettingsScreen::handleVoiceAssistantSettingsButton(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    Logger::getInstance().info("[Settings] Launching Voice Assistant settings...");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("VoiceAssistantSettings");
    }
}

void SettingsScreen::handleModeChanged(lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) {
        return;
    }
    int selected = lv_dropdown_get_selected(screen->mode_dropdown);
    OperatingMode_t mode;
    switch (selected) {
        case 0: mode = OPERATING_MODE_FULL; break;
        case 1: mode = OPERATING_MODE_UI_ONLY; break;
        case 2: mode = OPERATING_MODE_WEB_ONLY; break;
        default: mode = OPERATING_MODE_FULL; break;
    }
    SettingsManager::getInstance().setOperatingMode(mode);
    Logger::getInstance().info( (String("Operating mode changed to: ") + String((int)mode)).c_str() );
}

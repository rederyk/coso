#include "screens/wifi_settings_screen.h"
#include "core/app_manager.h"
#include "core/keyboard_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include <WiFi.h>

namespace {
lv_obj_t* create_card(lv_obj_t* parent, const char* title, lv_color_t bg_color) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_style_bg_color(card, bg_color, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    if (title) {
        lv_obj_t* title_lbl = lv_label_create(card);
        lv_label_set_text(title_lbl, title);
        lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xf0f0f0), 0);
    }

    return card;
}
} // namespace

WifiSettingsScreen::~WifiSettingsScreen() {
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
    if (status_timer) {
        lv_timer_del(status_timer);
        status_timer = nullptr;
    }
}

void WifiSettingsScreen::build(lv_obj_t* parent) {
    if (!parent) return;

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

    // Header with back button
    lv_obj_t* header_container = lv_obj_create(root);
    lv_obj_remove_style_all(header_container);
    lv_obj_set_width(header_container, lv_pct(100));
    lv_obj_set_height(header_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(header_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header_container, 0, 0);

    back_btn = lv_btn_create(header_container);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_add_event_cb(back_btn, handleBackButton, LV_EVENT_CLICKED, this);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    header_label = lv_label_create(header_container);
    lv_label_set_text(header_label, UI_SYMBOL_WIFI " WiFi Settings");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_pad_left(header_label, 12, 0);

    // Content container
    content_container = lv_obj_create(root);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_width(content_container, lv_pct(100));
    lv_obj_set_height(content_container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(content_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_pad_row(content_container, 12, 0);

    // Enable WiFi Card
    enable_card = lv_obj_create(content_container);
    lv_obj_set_width(enable_card, lv_pct(100));
    lv_obj_set_height(enable_card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(enable_card, lv_color_hex(0x1a2332), 0);
    lv_obj_set_style_border_width(enable_card, 0, 0);
    lv_obj_set_style_radius(enable_card, 12, 0);
    lv_obj_set_style_pad_all(enable_card, 12, 0);
    lv_obj_set_layout(enable_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(enable_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(enable_card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    enable_label = lv_label_create(enable_card);
    lv_label_set_text(enable_label, "Abilita WiFi");
    lv_obj_set_style_text_font(enable_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(enable_label, lv_color_hex(0xf0f0f0), 0);

    enable_switch = lv_switch_create(enable_card);
    lv_obj_add_event_cb(enable_switch, handleEnableToggle, LV_EVENT_VALUE_CHANGED, this);

    wifi_enabled = !snapshot.wifiSsid.empty();
    if (wifi_enabled) {
        lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
    }

    // Connection Status Card
    status_card = create_card(content_container, "Stato Connessione", lv_color_hex(0x1a2332));

    status_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(status_label, "Disconnesso");

    ip_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0xa0a0a0), 0);
    lv_label_set_text(ip_label, "IP: ---");

    rssi_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(rssi_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(rssi_label, lv_color_hex(0xa0a0a0), 0);
    lv_label_set_text(rssi_label, "Signal: ---");

    mac_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mac_label, lv_color_hex(0x808080), 0);
    lv_label_set_text_fmt(mac_label, "MAC: %s", WiFi.macAddress().c_str());

    // Configuration Card
    config_card = create_card(content_container, "Configurazione", lv_color_hex(0x1a2332));

    lv_obj_t* ssid_label = lv_label_create(config_card);
    lv_label_set_text(ssid_label, "SSID:");
    lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_14, 0);

    ssid_input = lv_textarea_create(config_card);
    lv_textarea_set_one_line(ssid_input, true);
    lv_textarea_set_max_length(ssid_input, 63);
    lv_textarea_set_placeholder_text(ssid_input, "Nome rete WiFi");
    lv_obj_set_width(ssid_input, lv_pct(100));
    lv_obj_add_event_cb(ssid_input, handleSsidInput, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(ssid_input, [](lv_event_t* e) {
        lv_obj_t* ta = lv_event_get_target(e);
        KeyboardManager::getInstance().showForTextArea(ta);
    }, LV_EVENT_FOCUSED, nullptr);

    lv_obj_t* pass_label = lv_label_create(config_card);
    lv_label_set_text(pass_label, "Password:");
    lv_obj_set_style_text_font(pass_label, &lv_font_montserrat_14, 0);

    password_input = lv_textarea_create(config_card);
    lv_textarea_set_one_line(password_input, true);
    lv_textarea_set_password_mode(password_input, true);
    lv_textarea_set_max_length(password_input, 63);
    lv_textarea_set_placeholder_text(password_input, "Password rete");
    lv_obj_set_width(password_input, lv_pct(100));
    lv_obj_add_event_cb(password_input, handlePasswordInput, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(password_input, [](lv_event_t* e) {
        lv_obj_t* ta = lv_event_get_target(e);
        KeyboardManager::getInstance().showForTextArea(ta);
    }, LV_EVENT_FOCUSED, nullptr);

    connect_btn = lv_btn_create(config_card);
    lv_obj_set_width(connect_btn, lv_pct(100));
    lv_obj_set_height(connect_btn, 50);
    lv_obj_add_event_cb(connect_btn, handleConnectButton, LV_EVENT_CLICKED, this);
    connect_btn_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_btn_label, "Connetti");
    lv_obj_center(connect_btn_label);
    lv_obj_set_style_text_font(connect_btn_label, &lv_font_montserrat_16, 0);

    // Network Scan Card
    scan_card = create_card(content_container, "Reti Disponibili", lv_color_hex(0x1a2332));

    scan_btn = lv_btn_create(scan_card);
    lv_obj_set_width(scan_btn, lv_pct(100));
    lv_obj_add_event_cb(scan_btn, handleScanButton, LV_EVENT_CLICKED, this);
    lv_obj_t* scan_btn_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_btn_label, LV_SYMBOL_REFRESH " Scansiona Reti");
    lv_obj_center(scan_btn_label);

    network_list = lv_list_create(scan_card);
    lv_obj_set_width(network_list, lv_pct(100));
    lv_obj_set_height(network_list, 150);
    lv_obj_set_style_bg_opa(network_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(network_list, 1, 0);
    lv_obj_set_style_border_color(network_list, lv_color_hex(0x404040), 0);

    // Apply current values
    applySnapshot(snapshot);
    applyThemeStyles(snapshot);

    // Create status update timer (every 2 seconds)
    status_timer = lv_timer_create(updateStatusTimer, 2000, this);

    // Register settings listener
    if (settings_listener_id == 0) {
        settings_listener_id = manager.addListener([this](SettingsManager::SettingKey, const SettingsSnapshot& snap) {
            if (!root) return;
            applySnapshot(snap);
        });
    }

    updateConnectionStatus();
}

void WifiSettingsScreen::onShow() {
    Logger::getInstance().info("[WiFi Settings] Screen shown");
    updateConnectionStatus();
}

void WifiSettingsScreen::onHide() {
    Logger::getInstance().info("[WiFi Settings] Screen hidden");
}

void WifiSettingsScreen::applySnapshot(const SettingsSnapshot& snapshot) {
    updating_from_manager = true;

    if (ssid_input) {
        lv_textarea_set_text(ssid_input, snapshot.wifiSsid.c_str());
    }
    if (password_input) {
        lv_textarea_set_text(password_input, snapshot.wifiPassword.c_str());
    }

    updating_from_manager = false;
}

void WifiSettingsScreen::applyThemeStyles(const SettingsSnapshot& snapshot) {
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
    }
    if (header_label) {
        lv_obj_set_style_text_color(header_label, accent, 0);
    }
    if (back_btn) {
        lv_obj_set_style_bg_color(back_btn, accent, 0);
    }
}

void WifiSettingsScreen::updateConnectionStatus() {
    if (!status_label) return;

    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        lv_label_set_text(status_label, "✓ Connesso");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00ff00), 0);

        if (ip_label) {
            lv_label_set_text_fmt(ip_label, "IP: %s", WiFi.localIP().toString().c_str());
        }
        if (rssi_label) {
            int32_t rssi = WiFi.RSSI();
            std::string icon = getRssiIcon(rssi);
            lv_label_set_text_fmt(rssi_label, "%s Signal: %d dBm", icon.c_str(), rssi);
        }
        if (connect_btn_label) {
            lv_label_set_text(connect_btn_label, "Disconnetti");
        }
    } else if (status == WL_CONNECT_FAILED) {
        lv_label_set_text(status_label, "✗ Connessione fallita");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xff0000), 0);
        if (ip_label) lv_label_set_text(ip_label, "IP: ---");
        if (rssi_label) lv_label_set_text(rssi_label, "Signal: ---");
        if (connect_btn_label) lv_label_set_text(connect_btn_label, "Riprova");
    } else if (status == WL_DISCONNECTED || status == WL_IDLE_STATUS) {
        lv_label_set_text(status_label, "○ Disconnesso");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xa0a0a0), 0);
        if (ip_label) lv_label_set_text(ip_label, "IP: ---");
        if (rssi_label) lv_label_set_text(rssi_label, "Signal: ---");
        if (connect_btn_label) lv_label_set_text(connect_btn_label, "Connetti");
    } else {
        // Connecting
        lv_label_set_text(status_label, "◌ Connessione in corso...");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xffaa00), 0);
    }
}

std::string WifiSettingsScreen::getRssiIcon(int32_t rssi) const {
    if (rssi > -50) return LV_SYMBOL_WIFI;
    if (rssi > -70) return LV_SYMBOL_WIFI;
    if (rssi > -80) return LV_SYMBOL_WIFI;
    return LV_SYMBOL_WIFI;
}

void WifiSettingsScreen::startNetworkScan() {
    if (is_scanning) return;
    is_scanning = true;

    // Clear existing list
    lv_obj_clean(network_list);

    // Show scanning message
    lv_obj_t* scanning_item = lv_list_add_btn(network_list, LV_SYMBOL_REFRESH, "Scansione in corso...");
    lv_obj_set_style_bg_opa(scanning_item, LV_OPA_TRANSP, 0);

    // Start async scan
    WiFi.scanNetworks(true);
}

void WifiSettingsScreen::updateNetworkList(const std::vector<NetworkInfo>& networks) {
    if (!network_list) return;

    lv_obj_clean(network_list);

    if (networks.empty()) {
        lv_obj_t* empty_item = lv_list_add_btn(network_list, LV_SYMBOL_WARNING, "Nessuna rete trovata");
        lv_obj_set_style_bg_opa(empty_item, LV_OPA_TRANSP, 0);
        return;
    }

    for (const auto& network : networks) {
        std::string icon = getRssiIcon(network.rssi);
        std::string lock = network.encrypted ? " [L]" : "";
        std::string label = network.ssid + lock + " (" + std::to_string(network.rssi) + " dBm)";

        lv_obj_t* btn = lv_list_add_btn(network_list, icon.c_str(), label.c_str());
        lv_obj_add_event_cb(btn, handleNetworkSelected, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, (void*)network.ssid.c_str());
    }
}

void WifiSettingsScreen::handleEnableToggle(lv_event_t* e) {
    auto* screen = static_cast<WifiSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    bool enabled = lv_obj_has_state(screen->enable_switch, LV_STATE_CHECKED);
    screen->wifi_enabled = enabled;

    if (!enabled) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        Logger::getInstance().info("[WiFi] Disabled");
    } else {
        WiFi.mode(WIFI_STA);
        Logger::getInstance().info("[WiFi] Enabled");
    }

    screen->updateConnectionStatus();
}

void WifiSettingsScreen::handleSsidInput(lv_event_t* e) {
    auto* screen = static_cast<WifiSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    const char* text = lv_textarea_get_text(screen->ssid_input);
    SettingsManager::getInstance().setWifiSsid(text ? text : "");
}

void WifiSettingsScreen::handlePasswordInput(lv_event_t* e) {
    auto* screen = static_cast<WifiSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    const char* text = lv_textarea_get_text(screen->password_input);
    SettingsManager::getInstance().setWifiPassword(text ? text : "");
}

void WifiSettingsScreen::handleConnectButton(lv_event_t* e) {
    auto* screen = static_cast<WifiSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        // Disconnect
        WiFi.disconnect();
        Logger::getInstance().info("[WiFi] Disconnecting...");
    } else {
        // Connect
        const SettingsSnapshot& snapshot = SettingsManager::getInstance().getSnapshot();
        if (!snapshot.wifiSsid.empty()) {
            WiFi.begin(snapshot.wifiSsid.c_str(), snapshot.wifiPassword.c_str());
            Logger::getInstance().infof("[WiFi] Connecting to: %s", snapshot.wifiSsid.c_str());
        } else {
            Logger::getInstance().warn("[WiFi] No SSID configured");
        }
    }

    screen->updateConnectionStatus();
}

void WifiSettingsScreen::handleScanButton(lv_event_t* e) {
    auto* screen = static_cast<WifiSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    screen->startNetworkScan();
}

void WifiSettingsScreen::handleNetworkSelected(lv_event_t* e) {
    auto* screen = static_cast<WifiSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    lv_obj_t* btn = lv_event_get_target(e);
    const char* ssid = (const char*)lv_obj_get_user_data(btn);

    if (ssid && screen->ssid_input) {
        lv_textarea_set_text(screen->ssid_input, ssid);
        SettingsManager::getInstance().setWifiSsid(ssid);
        Logger::getInstance().infof("[WiFi] Selected network: %s", ssid);
    }
}

void WifiSettingsScreen::handleBackButton(lv_event_t* e) {
    auto* screen = static_cast<WifiSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    // Navigate back to settings screen
    Logger::getInstance().info("[WiFi Settings] Returning to Settings...");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("settings");
    }
}

void WifiSettingsScreen::updateStatusTimer(lv_timer_t* timer) {
    auto* screen = static_cast<WifiSettingsScreen*>(timer->user_data);
    if (!screen) return;

    screen->updateConnectionStatus();

    // Check if scan completed
    if (screen->is_scanning) {
        int16_t scan_result = WiFi.scanComplete();
        if (scan_result >= 0) {
            // Scan completed
            std::vector<NetworkInfo> networks;
            for (int i = 0; i < scan_result; ++i) {
                NetworkInfo info;
                info.ssid = WiFi.SSID(i).c_str();
                info.rssi = WiFi.RSSI(i);
                info.encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
                networks.push_back(info);
            }
            screen->updateNetworkList(networks);
            screen->is_scanning = false;
            WiFi.scanDelete();
        }
    }
}

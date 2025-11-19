#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"
#include <lvgl.h>
#include <vector>
#include <string>

/**
 * @brief WiFi configuration screen with advanced controls
 *
 * Features:
 * - Enable/Disable WiFi
 * - SSID/Password input
 * - Connect/Disconnect button
 * - Network scanning
 * - Real-time connection status
 * - Signal strength (RSSI)
 * - IP address display
 */
class WifiSettingsScreen : public Screen {
public:
    ~WifiSettingsScreen() override;
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    struct NetworkInfo {
        std::string ssid;
        int32_t rssi;
        bool encrypted;
    };

    void applySnapshot(const SettingsSnapshot& snapshot);
    void applyThemeStyles(const SettingsSnapshot& snapshot);
    void updateConnectionStatus();
    void updateNetworkList(const std::vector<NetworkInfo>& networks);
    void startNetworkScan();
    std::string getRssiIcon(int32_t rssi) const;
    std::string getConnectionStatusText() const;

    // Event handlers
    static void handleEnableToggle(lv_event_t* e);
    static void handleSsidInput(lv_event_t* e);
    static void handlePasswordInput(lv_event_t* e);
    static void handleConnectButton(lv_event_t* e);
    static void handleScanButton(lv_event_t* e);
    static void handleNetworkSelected(lv_event_t* e);
    static void handleBackButton(lv_event_t* e);
    static void updateStatusTimer(lv_timer_t* timer);

    // UI components
    lv_obj_t* header_label = nullptr;
    lv_obj_t* back_btn = nullptr;
    lv_obj_t* content_container = nullptr;

    // WiFi Enable Card
    lv_obj_t* enable_card = nullptr;
    lv_obj_t* enable_switch = nullptr;
    lv_obj_t* enable_label = nullptr;

    // Connection Status Card
    lv_obj_t* status_card = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* ip_label = nullptr;
    lv_obj_t* rssi_label = nullptr;
    lv_obj_t* mac_label = nullptr;

    // Configuration Card
    lv_obj_t* config_card = nullptr;
    lv_obj_t* ssid_input = nullptr;
    lv_obj_t* password_input = nullptr;
    lv_obj_t* connect_btn = nullptr;
    lv_obj_t* connect_btn_label = nullptr;

    // Network Scan Card
    lv_obj_t* scan_card = nullptr;
    lv_obj_t* scan_btn = nullptr;
    lv_obj_t* network_list = nullptr;
    lv_obj_t* scan_spinner = nullptr;

    // State
    bool updating_from_manager = false;
    bool wifi_enabled = false;
    bool is_scanning = false;
    lv_timer_t* status_timer = nullptr;
    uint32_t settings_listener_id = 0;
};

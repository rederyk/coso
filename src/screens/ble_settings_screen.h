#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"
#include <lvgl.h>

/**
 * @brief BLE configuration screen with advanced controls
 *
 * Features:
 * - Enable/Disable BLE
 * - Device name configuration
 * - Connection status display
 * - Start/Stop advertising
 * - Service UUID display
 * - Connected clients count
 */
class BleSettingsScreen : public Screen {
public:
    ~BleSettingsScreen() override;
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    void applySnapshot(const SettingsSnapshot& snapshot);
    void applyThemeStyles(const SettingsSnapshot& snapshot);
    void updateBleStatus();

    // Event handlers
    static void handleEnableToggle(lv_event_t* e);
    static void handleDeviceNameInput(lv_event_t* e);
    static void handleAdvertisingToggle(lv_event_t* e);
    static void handleBackButton(lv_event_t* e);
    static void updateStatusTimer(lv_timer_t* timer);

    // UI components
    lv_obj_t* header_label = nullptr;
    lv_obj_t* back_btn = nullptr;
    lv_obj_t* content_container = nullptr;

    // Enable BLE Card
    lv_obj_t* enable_card = nullptr;
    lv_obj_t* enable_switch = nullptr;
    lv_obj_t* enable_label = nullptr;

    // Status Card
    lv_obj_t* status_card = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* clients_label = nullptr;
    lv_obj_t* mac_label = nullptr;

    // Configuration Card
    lv_obj_t* config_card = nullptr;
    lv_obj_t* device_name_input = nullptr;
    lv_obj_t* service_uuid_label = nullptr;
    lv_obj_t* char_uuid_label = nullptr;

    // Advertising Card
    lv_obj_t* advertising_card = nullptr;
    lv_obj_t* advertising_switch = nullptr;
    lv_obj_t* advertising_status_label = nullptr;

    // Info Card
    lv_obj_t* info_card = nullptr;
    lv_obj_t* info_label = nullptr;

    // State
    bool updating_from_manager = false;
    bool ble_enabled = false;
    bool is_advertising = false;
    lv_timer_t* status_timer = nullptr;
    uint32_t settings_listener_id = 0;
};

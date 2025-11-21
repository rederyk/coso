#pragma once

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <string>
#include <vector>
#if defined(CONFIG_NIMBLE_CPP_IDF)
#include "host/ble_gap.h"
#else
#include "nimble/nimble/host/include/host/ble_gap.h"
#endif

class BleHidServerCallbacks;

/**
 * @brief Singleton per gestire BLE HID (tastiera + mouse) via NimBLE
 */
class BleHidManager {
public:
    static BleHidManager& getInstance();
    ~BleHidManager() = default;

    bool init(const std::string& device_name = "ESP32-S3 HID");
    void startAdvertising();
    void stopAdvertising();
    void setEnabled(bool enable);
    bool isInitialized() const { return initialized_; }
    bool isEnabled() const { return enabled_; }
    bool isAdvertising() const { return is_advertising_; }
    bool isConnected() const { return is_connected_; }

    bool sendText(const std::string& text);
    bool sendKey(uint8_t keycode, uint8_t modifier = 0);
    bool sendMouseMove(int8_t dx, int8_t dy, int8_t wheel = 0, uint8_t buttons = 0);
    void click(uint8_t buttons);
    void disconnectAll();

    struct BondedPeer {
        NimBLEAddress address;
        bool isConnected = false;

        BondedPeer() = default;
        BondedPeer(const NimBLEAddress& addr, bool connected) : address(addr), isConnected(connected) {}
    };

    std::vector<BondedPeer> getBondedPeers() const;
    bool forgetPeer(const NimBLEAddress& address);
    bool startDirectedAdvertisingTo(const NimBLEAddress& address, uint32_t timeout_seconds = 15);
    bool isAdvertisingDirected() const { return is_directed_advertising_; }
    std::string getCurrentPeerAddress() const { return current_peer_address_; }
    std::string getDirectedTarget() const { return is_directed_advertising_ ? directed_target_.toString() : std::string(); }

    // Internal callbacks (exposed for server callbacks)
    void handleServerConnect(ble_gap_conn_desc* desc = nullptr);
    void handleServerDisconnect(ble_gap_conn_desc* desc = nullptr);

    std::string getAddress() const;
    std::string getDeviceName() const { return device_name_; }
    void setDeviceName(const std::string& name);

private:
    BleHidManager() = default;
    BleHidManager(const BleHidManager&) = delete;
    BleHidManager& operator=(const BleHidManager&) = delete;

    struct KeyMapping {
        uint8_t keycode;
        uint8_t modifier;
        bool valid;
    };

    KeyMapping mapCharToKey(char c) const;
    void updateLedState();
    void ensureAdvertising();

    static constexpr uint8_t KEYBOARD_ID = 0x01;
    static constexpr uint8_t MOUSE_ID = 0x02;

    bool initialized_ = false;
    bool is_advertising_ = false;
    bool is_connected_ = false;
    bool enabled_ = true;
    bool is_directed_advertising_ = false;
    std::string device_name_ = "ESP32-S3 HID";
    std::string current_peer_address_;
    uint16_t conn_handle_ = 0xFFFF; // NimBLE invalid conn handle placeholder

    NimBLEServer* server_ = nullptr;
    NimBLEHIDDevice* hid_device_ = nullptr;
    NimBLECharacteristic* input_keyboard_ = nullptr;
    NimBLECharacteristic* input_mouse_ = nullptr;
    NimBLEAddress directed_target_;

    friend class BleHidServerCallbacks;
};

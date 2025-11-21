#pragma once

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <string>

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

    // Internal callbacks (exposed for server callbacks)
    void handleServerConnect();
    void handleServerDisconnect();

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
    std::string device_name_ = "ESP32-S3 HID";

    NimBLEServer* server_ = nullptr;
    NimBLEHIDDevice* hid_device_ = nullptr;
    NimBLECharacteristic* input_keyboard_ = nullptr;
    NimBLECharacteristic* input_mouse_ = nullptr;

    friend class BleHidServerCallbacks;
};

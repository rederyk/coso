#pragma once

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <string>
#include <vector>
#include <sdkconfig.h>
#if defined(CONFIG_NIMBLE_CPP_IDF)
#include "host/ble_gap.h"
#else
#include "nimble/nimble/host/include/host/ble_gap.h"
#endif

/**
 * @brief Target selection for HID notifications.
 *
 * ALL: send to every connected peer.
 * FIRST_CONNECTED: deprecated, now treated as ALL.
 * LAST_CONNECTED: deprecated, now treated as ALL.
 */
enum class BleHidTarget : uint8_t {
    ALL,
    FIRST_CONNECTED,
    LAST_CONNECTED
};

class BleHidServerCallbacks;
class BleManager;  // Forward declaration

/**
 * @brief Singleton per gestire BLE HID (tastiera + mouse) via NimBLE
 *
 * NOTE: This class should not be accessed directly from the UI thread.
 * Use BleManager to post commands to the BLE task instead.
 */
class BleHidManager {
public:
    static BleHidManager& getInstance();
    ~BleHidManager() = default;

    // Read-only status methods (thread-safe)
    bool isInitialized() const { return initialized_; }
    bool isEnabled() const { return enabled_; }
    bool isAdvertising() const { return is_advertising_; }
    bool isConnected() const { return !connected_peers_.empty(); }
    size_t getConnectedCount() const { return connected_peers_.size(); }
    bool isAdvertisingDirected() const { return is_directed_advertising_; }
    size_t getMaxConnectionsAllowed() const { return max_connections_allowed_; }
    std::string getAddress() const;
    std::string getDeviceName() const { return device_name_; }
    std::string getDirectedTarget() const { return is_directed_advertising_ ? directed_target_.toString() : std::string(); }

    struct BondedPeer {
        NimBLEAddress address;
        bool isConnected = false;

        BondedPeer() = default;
        BondedPeer(const NimBLEAddress& addr, bool connected) : address(addr), isConnected(connected) {}
    };

    // Read-only query methods (thread-safe)
    std::vector<BondedPeer> getBondedPeers() const;
    std::vector<std::string> getConnectedPeerAddresses() const;

    // Internal callbacks (exposed for server callbacks)
    void handleServerConnect(ble_gap_conn_desc* desc = nullptr);
    void handleServerDisconnect(ble_gap_conn_desc* desc = nullptr);

private:
    BleHidManager() = default;
    BleHidManager(const BleHidManager&) = delete;
    BleHidManager& operator=(const BleHidManager&) = delete;

    // BleManager is the only class that should call these methods
    friend class BleManager;
    friend class BleHidServerCallbacks;

    // Control methods - private, only accessible via BleManager
    bool init(const std::string& device_name = "ESP32-S3 HID");
    void startAdvertising();
    void stopAdvertising();
    void setAdvertisingAllowed(bool allowed);
    void setAutoAdvertising(bool enabled);
    void setEnabled(bool enable);
    void setDeviceName(const std::string& name);
    void setMaxConnections(uint8_t max_connections);
    void disconnectAll();
    void disconnect(uint16_t conn_handle);
    bool forgetPeer(const NimBLEAddress& address);
    bool startDirectedAdvertisingTo(const NimBLEAddress& address, uint32_t timeout_seconds = 15);

    // HID methods - private, only accessible via BleManager
    bool sendText(const std::string& text, BleHidTarget target = BleHidTarget::ALL, const std::string& specific_mac = "");
    bool sendKey(uint8_t keycode, uint8_t modifier = 0, BleHidTarget target = BleHidTarget::ALL, const std::string& specific_mac = "");
    bool sendMouseMove(int8_t dx, int8_t dy, int8_t wheel = 0, uint8_t buttons = 0, BleHidTarget target = BleHidTarget::ALL, const std::string& specific_mac = "");
    void click(uint8_t buttons, BleHidTarget target = BleHidTarget::ALL, const std::string& specific_mac = "");

    struct KeyMapping {
        uint8_t keycode;
        uint8_t modifier;
        bool valid;
    };

    KeyMapping mapCharToKey(char c) const;
    void updateLedState();
    void ensureAdvertising();
    std::vector<uint16_t> selectTargetHandles(BleHidTarget target, const std::string& specific_mac = "") const;
    bool notifyHandles(NimBLECharacteristic* chr, const uint8_t* data, size_t len, const std::vector<uint16_t>& handles);

    static constexpr uint8_t KEYBOARD_ID = 0x01;
    static constexpr uint8_t MOUSE_ID = 0x02;

    struct ConnectedPeer {
        uint16_t conn_handle;
        std::string address;
        uint32_t last_disconnect_time = 0;  // millis() when last disconnected
    };

    bool initialized_ = false;
    bool is_advertising_ = false;
    bool enabled_ = true;
    bool is_directed_advertising_ = false;
    bool advertising_allowed_ = true;
    bool auto_advertising_ = true;  // Auto restart advertising after disconnect
    size_t max_connections_allowed_ = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
    std::string device_name_ = "ESP32-S3 HID";
    std::vector<ConnectedPeer> connected_peers_;
    std::vector<ConnectedPeer> recent_disconnects_;  // Track recent disconnections

    NimBLEServer* server_ = nullptr;
    NimBLEHIDDevice* hid_device_ = nullptr;
    NimBLECharacteristic* input_keyboard_ = nullptr;
    NimBLECharacteristic* input_mouse_ = nullptr;
    NimBLEAddress directed_target_;

    friend class BleHidServerCallbacks;
};

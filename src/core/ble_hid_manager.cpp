#include "ble_hid_manager.h"
#include "drivers/rgb_led_driver.h"
#include "utils/logger.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class BleHidServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit BleHidServerCallbacks(BleHidManager& mgr) : manager_(mgr) {}

    void onConnect(NimBLEServer* pServer) override {
        (void)pServer;
    }

    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        (void)pServer;
        manager_.handleServerConnect(desc);
    }

    void onDisconnect(NimBLEServer* pServer) override {
        (void)pServer;
    }

    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        (void)pServer;
        manager_.handleServerDisconnect(desc);
    }

private:
    BleHidManager& manager_;
};

namespace {
const uint8_t kReportMap[] = {
    // Keyboard (Report ID 1)
    0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,       // USAGE (Keyboard)
    0xA1, 0x01,       // COLLECTION (Application)
    0x85, 0x01,       //   REPORT_ID (1)
    0x05, 0x07,       //   USAGE_PAGE (Keyboard)
    0x19, 0xE0,       //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xE7,       //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x25, 0x01,       //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,       //   REPORT_SIZE (1)
    0x95, 0x08,       //   REPORT_COUNT (8)
    0x81, 0x02,       //   INPUT (Data,Var,Abs)
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x81, 0x03,       //   INPUT (Cnst,Var,Abs) ; padding
    0x95, 0x05,       //   REPORT_COUNT (5)
    0x75, 0x01,       //   REPORT_SIZE (1)
    0x05, 0x08,       //   USAGE_PAGE (LEDs)
    0x19, 0x01,       //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,       //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,       //   OUTPUT (Data,Var,Abs)
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x75, 0x03,       //   REPORT_SIZE (3)
    0x91, 0x03,       //   OUTPUT (Cnst,Var,Abs)
    0x95, 0x06,       //   REPORT_COUNT (6)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x25, 0x65,       //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,       //   USAGE_PAGE (Keyboard)
    0x19, 0x00,       //   USAGE_MINIMUM (Reserved)
    0x29, 0x65,       //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,       //   INPUT (Data,Ary,Abs)
    0xC0,             // END_COLLECTION

    // Mouse (Report ID 2)
    0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,       // USAGE (Mouse)
    0xA1, 0x01,       // COLLECTION (Application)
    0x85, 0x02,       //   REPORT_ID (2)
    0x09, 0x01,       //   USAGE (Pointer)
    0xA1, 0x00,       //   COLLECTION (Physical)
    0x05, 0x09,       //     USAGE_PAGE (Button)
    0x19, 0x01,       //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,       //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,       //     LOGICAL_MINIMUM (0)
    0x25, 0x01,       //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,       //     REPORT_COUNT (3)
    0x75, 0x01,       //     REPORT_SIZE (1)
    0x81, 0x02,       //     INPUT (Data,Var,Abs)
    0x95, 0x01,       //     REPORT_COUNT (1)
    0x75, 0x05,       //     REPORT_SIZE (5)
    0x81, 0x03,       //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,       //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,       //     USAGE (X)
    0x09, 0x31,       //     USAGE (Y)
    0x09, 0x38,       //     USAGE (Wheel)
    0x15, 0x81,       //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,       //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,       //     REPORT_SIZE (8)
    0x95, 0x03,       //     REPORT_COUNT (3)
    0x81, 0x06,       //     INPUT (Data,Var,Rel)
    0xC0,             //   END_COLLECTION
    0xC0              // END_COLLECTION
};

// Minimal HID keycode/modifier map for ASCII basics
constexpr uint8_t KEYBOARD_MODIFIER_LEFTSHIFT = 0x02;
constexpr uint8_t KEY_A = 0x04;
constexpr uint8_t KEY_1 = 0x1E;
constexpr uint8_t KEY_2 = 0x1F;
constexpr uint8_t KEY_3 = 0x20;
constexpr uint8_t KEY_4 = 0x21;
constexpr uint8_t KEY_5 = 0x22;
constexpr uint8_t KEY_6 = 0x23;
constexpr uint8_t KEY_7 = 0x24;
constexpr uint8_t KEY_8 = 0x25;
constexpr uint8_t KEY_9 = 0x26;
constexpr uint8_t KEY_0 = 0x27;
constexpr uint8_t KEY_RETURN = 0x28;
constexpr uint8_t KEY_SPACE = 0x2C;
constexpr uint8_t KEY_MINUS = 0x2D;
constexpr uint8_t KEY_EQUAL = 0x2E;
constexpr uint8_t KEY_COMMA = 0x36;
constexpr uint8_t KEY_PERIOD = 0x37;
constexpr uint8_t KEY_SLASH = 0x38;

bool is_adv_active() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    return adv && adv->isAdvertising();
}
} // namespace

BleHidManager& BleHidManager::getInstance() {
    static BleHidManager instance;
    return instance;
}

bool BleHidManager::init(const std::string& device_name) {
    if (initialized_) {
        return true;
    }

    device_name_ = device_name;

    NimBLEDevice::init(device_name_.c_str());
    NimBLEDevice::setSecurityAuth(true, false, true); // bonding + LESC, no MITM
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityPasskey(000000);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC);

    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(new BleHidServerCallbacks(*this));

    // Configure server for multiple connections
    server_->advertiseOnDisconnect(true);  // Re-advertise on disconnect

    hid_device_ = new NimBLEHIDDevice(server_);
    hid_device_->manufacturer()->setValue("Freenove");
    hid_device_->pnp(0x02, 0x045E, 0x028E, 0x0110);
    hid_device_->hidInfo(0x00, 0x02);

    input_keyboard_ = hid_device_->inputReport(KEYBOARD_ID);
    input_mouse_ = hid_device_->inputReport(MOUSE_ID);
    if (!input_keyboard_ || !input_mouse_) {
        Logger::getInstance().error("[BLE HID] Failed to create input reports");
        return false;
    }

    hid_device_->reportMap(const_cast<uint8_t*>(kReportMap), sizeof(kReportMap));
    hid_device_->setBatteryLevel(100);
    hid_device_->startServices();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid_device_->hidService()->getUUID());
    advertising->setScanResponse(true);
    advertising->start();

    is_advertising_ = true;
    is_directed_advertising_ = false;
    advertising_allowed_ = true;
    initialized_ = true;
    updateLedState();

    Logger::getInstance().infof("[BLE HID] Initialized as '%s'", device_name_.c_str());
    Logger::getInstance().infof("[BLE HID] Address: %s", getAddress().c_str());
    return true;
}

void BleHidManager::startAdvertising() {
    if (!initialized_) return;
    constexpr size_t max_connections = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
    if (connected_peers_.size() >= max_connections) {
        Logger::getInstance().warn("[BLE HID] Skipping advertising: max connections reached");
        return;
    }

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (!advertising) return;

    if (advertising->isAdvertising()) {
        // Already advertising; keep flags consistent and skip churn.
        is_advertising_ = true;
        is_directed_advertising_ = false;
        directed_target_ = NimBLEAddress();
        return;
    }

    // Stop first to reset state
    advertising->stop();
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // Configure advertising parameters for multiple connections
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x20); // 40ms - more stable, less aggressive
    advertising->setMaxPreferred(0x40); // 80ms - standard connection interval

    // Start advertising
    advertising->start();

    is_advertising_ = true;
    is_directed_advertising_ = false;
    directed_target_ = NimBLEAddress();
    updateLedState();
    Logger::getInstance().info("[BLE HID] Advertising started");
}

void BleHidManager::stopAdvertising() {
    if (!initialized_) return;
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->stop();
    is_advertising_ = false;
    is_directed_advertising_ = false;
    directed_target_ = NimBLEAddress();
    updateLedState();
    Logger::getInstance().info("[BLE HID] Advertising stopped");
}

void BleHidManager::setAdvertisingAllowed(bool allowed) {
    advertising_allowed_ = allowed;
    if (!initialized_) return;

    if (!allowed) {
        NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
        if (advertising) {
            advertising->stop();
        }
        is_advertising_ = false;
        is_directed_advertising_ = false;
        directed_target_ = NimBLEAddress();
        updateLedState();
    } else {
        ensureAdvertising();
    }
}

void BleHidManager::setEnabled(bool enable) {
    enabled_ = enable;
    if (!initialized_) return;

    if (enable) {
        ensureAdvertising();
    } else {
        stopAdvertising();
        disconnectAll();
    }
}

void BleHidManager::ensureAdvertising() {
    if (!initialized_) return;
    if (!enabled_) return;
    if (!advertising_allowed_) return;
    constexpr size_t max_connections = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
    if (connected_peers_.size() >= max_connections) return;
    // Sync with stack state to avoid redundant start/stop churn.
    is_advertising_ = is_adv_active();
    // Continue advertising even with connections for multi-host support
    if (is_directed_advertising_) return;
    if (is_advertising_) return;
    startAdvertising();
}

void BleHidManager::handleServerConnect(ble_gap_conn_desc* desc) {
    if (desc) {
        ConnectedPeer peer;
        peer.conn_handle = desc->conn_handle;
        peer.address = NimBLEAddress(desc->peer_ota_addr).toString();

        // Check if this peer recently disconnected (within last 2 seconds)
        uint32_t now = millis();
        constexpr uint32_t reconnect_throttle_ms = 2000;
        auto recent = std::find_if(recent_disconnects_.begin(), recent_disconnects_.end(),
            [&](const ConnectedPeer& p) { return p.address == peer.address; });
        if (recent != recent_disconnects_.end() && (now - recent->last_disconnect_time) < reconnect_throttle_ms) {
            // Too soon after disconnect - reject this connection to break the loop
            Logger::getInstance().warnf("[BLE HID] Throttling rapid reconnect from %s (%u ms since disconnect)",
                peer.address.c_str(), now - recent->last_disconnect_time);
            if (server_) {
                server_->disconnect(peer.conn_handle);
            }
            return;
        }

        // Check for existing connection with same address
        auto dup = std::find_if(connected_peers_.begin(), connected_peers_.end(),
            [&](const ConnectedPeer& p) { return p.address == peer.address; });
        if (dup != connected_peers_.end()) {
            // Same MAC already connected: replace old handle with new one (reconnection scenario)
            Logger::getInstance().warnf("[BLE HID] Duplicate connect from %s - replacing old handle %d with %d",
                peer.address.c_str(), dup->conn_handle, peer.conn_handle);
            dup->conn_handle = peer.conn_handle;
            return;
        }

        connected_peers_.push_back(peer);
        Logger::getInstance().infof("[BLE HID] Client connected (%s), total: %d", peer.address.c_str(), connected_peers_.size());

        // Remove from recent disconnects if present
        recent_disconnects_.erase(std::remove_if(recent_disconnects_.begin(), recent_disconnects_.end(),
            [&](const ConnectedPeer& p) { return p.address == peer.address; }), recent_disconnects_.end());
    } else {
        Logger::getInstance().info("[BLE HID] Client connected (no descriptor)");
    }

    // NimBLE stops advertising on connect; keep internal flags in sync so ensureAdvertising() can restart it.
    is_advertising_ = is_adv_active();
    // Stop directed advertising if active
    if (is_directed_advertising_) {
        is_directed_advertising_ = false;
        directed_target_ = NimBLEAddress();
    }

    // Continue advertising for more connections if below max
    constexpr size_t max_connections = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
    if (connected_peers_.size() < max_connections) {
        // Give adequate delay (longer) to stabilize the connection before restarting advertising
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ensureAdvertising();
    } else {
        // Max connections reached, stop advertising
        if (is_advertising_) {
            stopAdvertising();
        }
    }

    updateLedState();
}

void BleHidManager::handleServerDisconnect(ble_gap_conn_desc* desc) {
    if (desc) {
        std::string addr = NimBLEAddress(desc->peer_ota_addr).toString();
        uint32_t disconnect_time = millis();

        auto it = std::find_if(connected_peers_.begin(), connected_peers_.end(),
            [&](const ConnectedPeer& p) { return p.conn_handle == desc->conn_handle; });
        if (it != connected_peers_.end()) {
            // Track this disconnect for throttling
            ConnectedPeer recent;
            recent.address = it->address;
            recent.last_disconnect_time = disconnect_time;
            recent.conn_handle = 0;  // Not needed for tracking

            // Add to recent disconnects, keep only last 5
            recent_disconnects_.push_back(recent);
            if (recent_disconnects_.size() > 5) {
                recent_disconnects_.erase(recent_disconnects_.begin());
            }

            connected_peers_.erase(it);
            Logger::getInstance().infof("[BLE HID] Client disconnected (%s), remaining: %d", addr.c_str(), connected_peers_.size());
        } else {
            Logger::getInstance().infof("[BLE HID] Client disconnected (%s) - not in list", addr.c_str());
        }
        // Clean up any stale entries for this address
        connected_peers_.erase(std::remove_if(connected_peers_.begin(), connected_peers_.end(),
            [&](const ConnectedPeer& p) { return p.address == addr && p.conn_handle != desc->conn_handle; }), connected_peers_.end());
    } else {
        Logger::getInstance().info("[BLE HID] Client disconnected (no descriptor)");
    }

    // Advertising might have been auto-restarted by NimBLE; align our flags.
    is_advertising_ = is_adv_active();

    // Give a longer delay before restarting advertising to avoid rapid reconnection loops
    vTaskDelay(1500 / portTICK_PERIOD_MS);

    ensureAdvertising();
    updateLedState();
}

void BleHidManager::disconnectAll() {
    if (!server_) return;

    // Request disconnection for all peers; callbacks will clean up the list.
    for (const auto& peer : connected_peers_) {
        server_->disconnect(peer.conn_handle);
    }

    is_directed_advertising_ = false;
    ensureAdvertising();
    // Do not force-clear LED state here; let callbacks reflect the actual disconnects.
}

void BleHidManager::disconnect(uint16_t conn_handle) {
    if (!server_) return;
    server_->disconnect(conn_handle);
    auto it = std::find_if(connected_peers_.begin(), connected_peers_.end(),
        [conn_handle](const ConnectedPeer& p) { return p.conn_handle == conn_handle; });
    if (it != connected_peers_.end()) {
        Logger::getInstance().infof("[BLE HID] Disconnecting %s", it->address.c_str());
    }
}

std::string BleHidManager::getAddress() const {
    return NimBLEDevice::getAddress().toString();
}

void BleHidManager::setDeviceName(const std::string& name) {
    if (name.empty()) return;
    device_name_ = name;
    NimBLEDevice::setDeviceName(device_name_.c_str());
    Logger::getInstance().infof("[BLE HID] Device name set to '%s' (restart advertising to apply)", device_name_.c_str());
}

std::vector<BleHidManager::BondedPeer> BleHidManager::getBondedPeers() const {
    std::vector<BondedPeer> peers;
    int count = NimBLEDevice::getNumBonds();
    peers.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        NimBLEAddress addr = NimBLEDevice::getBondedAddress(i);
        if (addr == NimBLEAddress()) {
            continue;
        }
        std::string addr_str = addr.toString();
        bool connected = std::any_of(connected_peers_.begin(), connected_peers_.end(),
            [&addr_str](const ConnectedPeer& p) { return p.address == addr_str; });
        BondedPeer peer{addr, connected};
        peers.push_back(peer);
    }
    return peers;
}

std::vector<std::string> BleHidManager::getConnectedPeerAddresses() const {
    std::vector<std::string> addresses;
    addresses.reserve(connected_peers_.size());
    for (const auto& peer : connected_peers_) {
        addresses.push_back(peer.address);
    }
    return addresses;
}

bool BleHidManager::forgetPeer(const NimBLEAddress& address) {
    if (!initialized_) return false;

    // Check if this peer is currently connected
    std::string addr_str = address.toString();
    auto it = std::find_if(connected_peers_.begin(), connected_peers_.end(),
        [&addr_str](const ConnectedPeer& p) { return p.address == addr_str; });

    if (it != connected_peers_.end()) {
        Logger::getInstance().warnf("[BLE HID] Cannot forget currently connected peer %s", addr_str.c_str());
        return false;
    }

    bool removed = NimBLEDevice::deleteBond(address);
    if (removed) {
        Logger::getInstance().infof("[BLE HID] Bond removed for %s", address.toString().c_str());
    } else {
        Logger::getInstance().warnf("[BLE HID] Unable to remove bond for %s", address.toString().c_str());
    }
    return removed;
}

bool BleHidManager::startDirectedAdvertisingTo(const NimBLEAddress& address, uint32_t timeout_seconds) {
    if (!initialized_ || !enabled_ || !advertising_allowed_) return false;

    // Check if this peer is already connected
    std::string addr_str = address.toString();
    auto it = std::find_if(connected_peers_.begin(), connected_peers_.end(),
        [&addr_str](const ConnectedPeer& p) { return p.address == addr_str; });

    if (it != connected_peers_.end()) {
        Logger::getInstance().warnf("[BLE HID] Peer %s already connected", addr_str.c_str());
        return false;
    }

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->stop();

    directed_target_ = address;
    auto adv_done = [this](NimBLEAdvertising*) {
        is_advertising_ = false;
        is_directed_advertising_ = false;
        directed_target_ = NimBLEAddress();
        updateLedState();
        // If nobody connected within timeout, fall back to generic advertising
        ensureAdvertising();
    };

    bool started = advertising->start(timeout_seconds, adv_done, &directed_target_);
    if (!started) {
        is_advertising_ = false;
        is_directed_advertising_ = false;
        directed_target_ = NimBLEAddress();
        updateLedState();
        Logger::getInstance().warnf("[BLE HID] Directed advertising failed for %s", address.toString().c_str());
        ensureAdvertising();
        return false;
    }

    is_advertising_ = true;
    is_directed_advertising_ = true;
    updateLedState();
    Logger::getInstance().infof("[BLE HID] Directed advertising for %u s to %s", timeout_seconds, address.toString().c_str());
    return true;
}

BleHidManager::KeyMapping BleHidManager::mapCharToKey(char c) const {
    KeyMapping km{0, 0, true};
    if (c >= 'a' && c <= 'z') {
        km.keycode = KEY_A + (c - 'a');
    } else if (c >= 'A' && c <= 'Z') {
        km.keycode = KEY_A + (c - 'A');
        km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
    } else if (c >= '1' && c <= '9') {
        km.keycode = KEY_1 + (c - '1');
    } else if (c == '0') {
        km.keycode = KEY_0;
    } else {
        switch (c) {
            case ' ': km.keycode = KEY_SPACE; break;
            case '\n': km.keycode = KEY_RETURN; break;
            case '.': km.keycode = KEY_PERIOD; break;
            case ',': km.keycode = KEY_COMMA; break;
            case '-': km.keycode = KEY_MINUS; break;
            case '_': km.keycode = KEY_MINUS; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '=': km.keycode = KEY_EQUAL; break;
            case '+': km.keycode = KEY_EQUAL; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '/': km.keycode = KEY_SLASH; break;
            case '?': km.keycode = KEY_SLASH; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '!': km.keycode = KEY_1; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '@': km.keycode = KEY_2; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '#': km.keycode = KEY_3; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '$': km.keycode = KEY_4; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '%': km.keycode = KEY_5; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '^': km.keycode = KEY_6; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '&': km.keycode = KEY_7; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '*': km.keycode = KEY_8; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case '(': km.keycode = KEY_9; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            case ')': km.keycode = KEY_0; km.modifier = KEYBOARD_MODIFIER_LEFTSHIFT; break;
            default: km.valid = false; break;
        }
    }
    return km;
}

bool BleHidManager::sendKey(uint8_t keycode, uint8_t modifier) {
    if (!initialized_ || !input_keyboard_ || connected_peers_.empty()) {
        return false;
    }

    uint8_t report[8] = {modifier, 0, keycode, 0, 0, 0, 0, 0};
    input_keyboard_->setValue(report, sizeof(report));
    input_keyboard_->notify();

    // key release
    memset(report, 0, sizeof(report));
    input_keyboard_->setValue(report, sizeof(report));
    input_keyboard_->notify();
    return true;
}

bool BleHidManager::sendText(const std::string& text) {
    if (!initialized_ || !input_keyboard_ || connected_peers_.empty()) {
        return false;
    }

    for (char c : text) {
        KeyMapping km = mapCharToKey(c);
        if (!km.valid) {
            Logger::getInstance().warnf("[BLE HID] Unsupported char skipped: 0x%02X", static_cast<unsigned char>(c));
            continue;
        }
        if (!sendKey(km.keycode, km.modifier)) {
            return false;
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    return true;
}

bool BleHidManager::sendMouseMove(int8_t dx, int8_t dy, int8_t wheel, uint8_t buttons) {
    if (!initialized_ || !input_mouse_ || connected_peers_.empty()) {
        return false;
    }

    uint8_t report[4];
    report[0] = buttons;
    report[1] = static_cast<uint8_t>(dx);
    report[2] = static_cast<uint8_t>(dy);
    report[3] = static_cast<uint8_t>(wheel);
    input_mouse_->setValue(report, sizeof(report));
    input_mouse_->notify();
    return true;
}

void BleHidManager::click(uint8_t buttons) {
    sendMouseMove(0, 0, 0, buttons);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    sendMouseMove(0, 0, 0, 0);
}

void BleHidManager::updateLedState() {
    RgbLedManager& led = RgbLedManager::getInstance();
    if (!led.isInitialized()) return;

    if (!enabled_) {
        led.setState(RgbLedManager::LedState::OFF);
        return;
    }

    if (!connected_peers_.empty()) {
        led.setState(RgbLedManager::LedState::BLE_CONNECTED);
    } else if (is_advertising_) {
        led.setState(RgbLedManager::LedState::BLE_ADVERTISING);
    } else {
        led.setState(RgbLedManager::LedState::OFF);
    }
}

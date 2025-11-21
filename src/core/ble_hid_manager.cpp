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
        manager_.handleServerConnect();
    }

    void onDisconnect(NimBLEServer* pServer) override {
        (void)pServer;
        manager_.handleServerDisconnect();
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
    initialized_ = true;
    updateLedState();

    Logger::getInstance().infof("[BLE HID] Initialized as '%s'", device_name_.c_str());
    Logger::getInstance().infof("[BLE HID] Address: %s", getAddress().c_str());
    return true;
}

void BleHidManager::startAdvertising() {
    if (!initialized_) return;
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->start();
    is_advertising_ = true;
    updateLedState();
    Logger::getInstance().info("[BLE HID] Advertising started");
}

void BleHidManager::stopAdvertising() {
    if (!initialized_) return;
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->stop();
    is_advertising_ = false;
    updateLedState();
    Logger::getInstance().info("[BLE HID] Advertising stopped");
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
    if (is_connected_) return;
    if (is_advertising_) return;
    startAdvertising();
}

void BleHidManager::handleServerConnect() {
    Logger::getInstance().info("[BLE HID] Client connected");
    is_connected_ = true;
    stopAdvertising();
    updateLedState();
}

void BleHidManager::handleServerDisconnect() {
    Logger::getInstance().info("[BLE HID] Client disconnected");
    is_connected_ = false;
    ensureAdvertising();
    updateLedState();
}

void BleHidManager::disconnectAll() {
    is_connected_ = false;
    updateLedState();
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
    if (!initialized_ || !input_keyboard_ || !is_connected_) {
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
    if (!initialized_ || !input_keyboard_ || !is_connected_) {
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
    if (!initialized_ || !input_mouse_ || !is_connected_) {
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

    if (is_connected_) {
        led.setState(RgbLedManager::LedState::BLE_CONNECTED);
    } else if (is_advertising_) {
        led.setState(RgbLedManager::LedState::BLE_ADVERTISING);
    } else {
        led.setState(RgbLedManager::LedState::OFF);
    }
}

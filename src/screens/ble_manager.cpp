#include "ble_manager.h"
#include "core/ble_client_manager.h"
#include "core/settings_manager.h"
#include "utils/logger.h"
#include <cstring>

BleManager& BleManager::getInstance() {
    static BleManager instance;
    return instance;
}

BleManager::BleManager() {
    // Create command queue
    command_queue_ = xQueueCreate(QUEUE_LENGTH, sizeof(BleCommand));
    if (!command_queue_) {
        Logger::getInstance().error("[BleManager] Failed to create command queue");
    }
}

BleManager::~BleManager() {
    if (command_queue_) {
        vQueueDelete(command_queue_);
        command_queue_ = nullptr;
    }
}

void BleManager::start() {
    if (task_handle_) {
        Logger::getInstance().warn("[BleManager] Task already started.");
        return;
    }

    if (!command_queue_) {
        Logger::getInstance().error("[BleManager] Command queue not initialized");
        return;
    }

    // Create the task on Core 0, leaving Core 1 for the UI (LVGL) and main loop.
    xTaskCreatePinnedToCore(
        bleTask,          // Task function
        "BleTask",        // Name of the task
        8192,             // Stack size in words
        this,             // Task input parameter
        5,                // Priority of the task
        &task_handle_,    // Task handle
        0                 // Core where the task should run
    );
    Logger::getInstance().info("[BleManager] BLE task started on Core 0.");
}

bool BleManager::postCommand(const BleCommand& cmd, uint32_t timeout_ms) {
    if (!command_queue_) {
        Logger::getInstance().error("[BleManager] Command queue not available");
        return false;
    }

    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xQueueSend(command_queue_, &cmd, ticks) != pdTRUE) {
        Logger::getInstance().warn("[BleManager] Failed to post command (queue full)");
        return false;
    }

    return true;
}

void BleManager::bleTask(void* pvParameters) {
    BleManager* manager = static_cast<BleManager*>(pvParameters);
    BleHidManager& ble_hid = BleHidManager::getInstance();
    BleClientManager& ble_client = BleClientManager::getInstance();

    const SettingsSnapshot snapshot = SettingsManager::getInstance().getSnapshot();
    const std::string device_name = snapshot.bleDeviceName.empty() ? "ESP32-S3 HID" : snapshot.bleDeviceName;

    if (!ble_hid.init(device_name)) {
        Logger::getInstance().error("[BleManager] Failed to initialize BLE HID stack");
    } else {
        ble_hid.setAdvertisingAllowed(snapshot.bleAdvertising);
        ble_hid.setEnabled(snapshot.bleEnabled);
        ble_hid.ensureAdvertising();
    }

    // Initialize BLE Client Manager
    ble_client.init();

    Logger::getInstance().info("[BleManager] BLE task running");

    for (;;) {
        BleCommand cmd;

        // Wait for a command with timeout for periodic checks
        if (xQueueReceive(manager->command_queue_, &cmd, pdMS_TO_TICKS(2000)) == pdTRUE) {
            // Process the received command
            manager->processCommand(cmd);
        } else {
            // Timeout - perform periodic checks
            ble_hid.ensureAdvertising();
        }
    }
}

void BleManager::processCommand(const BleCommand& cmd) {
    BleHidManager& ble_hid = BleHidManager::getInstance();
    bool success = true;

    switch (cmd.type) {
        case BleCommandType::ENABLE:
            ble_hid.setEnabled(cmd.bool_param);
            Logger::getInstance().infof("[BleManager] BLE %s",
                cmd.bool_param ? "enabled" : "disabled");
            break;

        case BleCommandType::DISABLE:
            ble_hid.setEnabled(false);
            Logger::getInstance().info("[BleManager] BLE disabled");
            break;

        case BleCommandType::START_ADVERTISING:
            ble_hid.setAdvertisingAllowed(true);
            ble_hid.startAdvertising();
            Logger::getInstance().info("[BleManager] Advertising started");
            break;

        case BleCommandType::STOP_ADVERTISING:
            ble_hid.setAdvertisingAllowed(false);
            Logger::getInstance().info("[BleManager] Advertising stopped");
            break;

        case BleCommandType::SET_DEVICE_NAME:
            ble_hid.setDeviceName(cmd.str_param);
            Logger::getInstance().infof("[BleManager] Device name set to: %s", cmd.str_param);
            break;

        case BleCommandType::DISCONNECT_ALL:
            ble_hid.disconnectAll();
            Logger::getInstance().info("[BleManager] Disconnecting all peers");
            break;

        case BleCommandType::DISCONNECT_PEER:
            ble_hid.disconnect(cmd.uint16_param);
            Logger::getInstance().infof("[BleManager] Disconnecting peer handle: %d", cmd.uint16_param);
            break;

        case BleCommandType::START_DIRECTED_ADV:
            success = ble_hid.startDirectedAdvertisingTo(cmd.address_param, cmd.uint32_param);
            Logger::getInstance().infof("[BleManager] Directed advertising to %s: %s",
                cmd.address_param.toString().c_str(),
                success ? "started" : "failed");
            break;

        case BleCommandType::FORGET_PEER:
            success = ble_hid.forgetPeer(cmd.address_param);
            Logger::getInstance().infof("[BleManager] Forget peer %s: %s",
                cmd.address_param.toString().c_str(),
                success ? "success" : "failed");
            break;

        case BleCommandType::SEND_KEY:
            success = ble_hid.sendKey(cmd.uint8_param, cmd.uint8_param2, cmd.target, cmd.target_mac);
            if (!success) {
                Logger::getInstance().warn("[BleManager] Failed to send key");
            }
            break;

        case BleCommandType::SEND_TEXT:
            success = ble_hid.sendText(cmd.str_param, cmd.target, cmd.target_mac);
            if (!success) {
                Logger::getInstance().warn("[BleManager] Failed to send text");
            }
            break;

        case BleCommandType::SEND_MOUSE_MOVE:
            success = ble_hid.sendMouseMove(
                cmd.int8_param,
                cmd.int8_param2,
                cmd.int8_param3,
                cmd.uint8_param,
                cmd.target,
                cmd.target_mac
            );
            if (!success) {
                Logger::getInstance().warn("[BleManager] Failed to send mouse move");
            }
            break;

        case BleCommandType::MOUSE_CLICK:
            ble_hid.click(cmd.uint8_param, cmd.target, cmd.target_mac);
            break;

        // Client commands
        case BleCommandType::SCAN_START: {
            BleClientManager& client = BleClientManager::getInstance();
            success = client.startScan(cmd.uint32_param);
            Logger::getInstance().infof("[BleManager] Scan %s", success ? "started" : "failed");
            break;
        }

        case BleCommandType::SCAN_STOP: {
            BleClientManager& client = BleClientManager::getInstance();
            client.stopScan();
            Logger::getInstance().info("[BleManager] Scan stopped");
            break;
        }

        case BleCommandType::CLIENT_CONNECT: {
            BleClientManager& client = BleClientManager::getInstance();
            success = client.connectTo(cmd.address_param);
            Logger::getInstance().infof("[BleManager] Client connect %s",
                success ? "succeeded" : "failed");
            break;
        }

        case BleCommandType::CLIENT_DISCONNECT: {
            BleClientManager& client = BleClientManager::getInstance();
            client.disconnectClient();
            Logger::getInstance().info("[BleManager] Client disconnected");
            break;
        }

        default:
            Logger::getInstance().warnf("[BleManager] Unknown command type: %d", static_cast<int>(cmd.type));
            success = false;
            break;
    }

    // Call callback if provided
    if (cmd.callback) {
        cmd.callback(success);
    }
}

// Convenience methods
void BleManager::enable(bool enabled) {
    BleCommand cmd;
    cmd.type = BleCommandType::ENABLE;
    cmd.bool_param = enabled;
    postCommand(cmd);
}

void BleManager::startAdvertising() {
    BleCommand cmd;
    cmd.type = BleCommandType::START_ADVERTISING;
    postCommand(cmd);
}

void BleManager::stopAdvertising() {
    BleCommand cmd;
    cmd.type = BleCommandType::STOP_ADVERTISING;
    postCommand(cmd);
}

void BleManager::setDeviceName(const std::string& name) {
    BleCommand cmd;
    cmd.type = BleCommandType::SET_DEVICE_NAME;
    strncpy(cmd.str_param, name.c_str(), sizeof(cmd.str_param) - 1);
    cmd.str_param[sizeof(cmd.str_param) - 1] = '\0';
    postCommand(cmd);
}

void BleManager::disconnectAll() {
    BleCommand cmd;
    cmd.type = BleCommandType::DISCONNECT_ALL;
    postCommand(cmd);
}

void BleManager::disconnect(uint16_t conn_handle) {
    BleCommand cmd;
    cmd.type = BleCommandType::DISCONNECT_PEER;
    cmd.uint16_param = conn_handle;
    postCommand(cmd);
}

void BleManager::forgetPeer(const NimBLEAddress& address) {
    BleCommand cmd;
    cmd.type = BleCommandType::FORGET_PEER;
    cmd.address_param = address;
    postCommand(cmd);
}

void BleManager::startDirectedAdvertising(const NimBLEAddress& address, uint32_t timeout_seconds) {
    BleCommand cmd;
    cmd.type = BleCommandType::START_DIRECTED_ADV;
    cmd.address_param = address;
    cmd.uint32_param = timeout_seconds;
    postCommand(cmd);
}

void BleManager::sendKey(uint8_t keycode, uint8_t modifier, BleHidTarget target, const std::string& specific_mac) {
    BleCommand cmd;
    cmd.type = BleCommandType::SEND_KEY;
    cmd.uint8_param = keycode;
    cmd.uint8_param2 = modifier;
    cmd.target = target;
    if (!specific_mac.empty()) {
        strncpy(cmd.target_mac, specific_mac.c_str(), sizeof(cmd.target_mac) - 1);
        cmd.target_mac[sizeof(cmd.target_mac) - 1] = '\0';
    }
    postCommand(cmd);
}

void BleManager::sendText(const std::string& text, BleHidTarget target, const std::string& specific_mac) {
    BleCommand cmd;
    cmd.type = BleCommandType::SEND_TEXT;
    strncpy(cmd.str_param, text.c_str(), sizeof(cmd.str_param) - 1);
    cmd.str_param[sizeof(cmd.str_param) - 1] = '\0';
    cmd.target = target;
    if (!specific_mac.empty()) {
        strncpy(cmd.target_mac, specific_mac.c_str(), sizeof(cmd.target_mac) - 1);
        cmd.target_mac[sizeof(cmd.target_mac) - 1] = '\0';
    }
    postCommand(cmd);
}

void BleManager::sendMouseMove(int8_t dx, int8_t dy, int8_t wheel, uint8_t buttons, BleHidTarget target, const std::string& specific_mac) {
    BleCommand cmd;
    cmd.type = BleCommandType::SEND_MOUSE_MOVE;
    cmd.int8_param = dx;
    cmd.int8_param2 = dy;
    cmd.int8_param3 = wheel;
    cmd.uint8_param = buttons;
    cmd.target = target;
    if (!specific_mac.empty()) {
        strncpy(cmd.target_mac, specific_mac.c_str(), sizeof(cmd.target_mac) - 1);
        cmd.target_mac[sizeof(cmd.target_mac) - 1] = '\0';
    }
    postCommand(cmd);
}

void BleManager::mouseClick(uint8_t buttons, BleHidTarget target, const std::string& specific_mac) {
    BleCommand cmd;
    cmd.type = BleCommandType::MOUSE_CLICK;
    cmd.uint8_param = buttons;
    cmd.target = target;
    if (!specific_mac.empty()) {
        strncpy(cmd.target_mac, specific_mac.c_str(), sizeof(cmd.target_mac) - 1);
        cmd.target_mac[sizeof(cmd.target_mac) - 1] = '\0';
    }
    postCommand(cmd);
}

// Client/Central convenience methods
void BleManager::startScan(uint32_t duration_ms) {
    BleCommand cmd;
    cmd.type = BleCommandType::SCAN_START;
    cmd.uint32_param = duration_ms;
    postCommand(cmd);
}

void BleManager::stopScan() {
    BleCommand cmd;
    cmd.type = BleCommandType::SCAN_STOP;
    postCommand(cmd);
}

void BleManager::connectToDevice(const NimBLEAddress& address) {
    BleCommand cmd;
    cmd.type = BleCommandType::CLIENT_CONNECT;
    cmd.address_param = address;
    postCommand(cmd);
}

void BleManager::disconnectFromDevice() {
    BleCommand cmd;
    cmd.type = BleCommandType::CLIENT_DISCONNECT;
    postCommand(cmd);
}

void BleManager::clearScanResults() {
    BleClientManager::getInstance().clearScanResults();
}

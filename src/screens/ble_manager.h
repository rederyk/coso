#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "core/ble_hid_manager.h"
#include "core/ble_client_manager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <NimBLEDevice.h>
#include <string>
#include <functional>

/**
 * @enum BleCommandType
 * @brief Types of commands that can be posted to the BLE task
 */
enum class BleCommandType {
    // Server/Peripheral commands
    ENABLE,
    DISABLE,
    START_ADVERTISING,
    STOP_ADVERTISING,
    SET_AUTO_ADVERTISING,
    SET_DEVICE_NAME,
    SET_MAX_CONNECTIONS,
    DISCONNECT_ALL,
    DISCONNECT_PEER,
    START_DIRECTED_ADV,
    FORGET_PEER,

    // HID commands
    SEND_KEY,
    SEND_TEXT,
    SEND_MOUSE_MOVE,
    MOUSE_CLICK,

    // Client/Central commands (for future use)
    SCAN_START,
    SCAN_STOP,
    CLIENT_CONNECT,
    CLIENT_DISCONNECT
};

/**
 * @struct BleCommand
 * @brief Command structure for the BLE task queue
 */
struct BleCommand {
    BleCommandType type;

    // Simple data fields (avoiding union with non-trivial types)
    bool bool_param;
    uint8_t uint8_param;
    uint8_t uint8_param2;
    uint16_t uint16_param;
    uint32_t uint32_param;
    int8_t int8_param;
    int8_t int8_param2;
    int8_t int8_param3;
    char str_param[128];  // For text and names
    NimBLEAddress address_param;  // For BLE addresses
    BleHidTarget target;          // Target peer for HID commands
    char target_mac[32];          // Specific MAC address for HID target

    // Optional callback for async responses
    std::function<void(bool success)> callback;

    // Constructor
    BleCommand() : type(BleCommandType::ENABLE), bool_param(false), uint8_param(0),
                   uint8_param2(0), uint16_param(0), uint32_param(0),
                   int8_param(0), int8_param2(0), int8_param3(0),
                   address_param(), target(BleHidTarget::ALL) {
        memset(str_param, 0, sizeof(str_param));
        memset(target_mac, 0, sizeof(target_mac));
    }
};

/**
 * @class BleManager
 * @brief Singleton to manage the BLE stack in a dedicated FreeRTOS task.
 *
 * This manager initializes and runs the BLE HID functionality in a separate
 * task to prevent blocking the main UI loop and to provide a clear
 * separation of concerns. Commands are sent via a FreeRTOS queue to avoid
 * direct calls from the UI thread.
 */
class BleManager {
public:
    static BleManager& getInstance();

    void start();
    bool postCommand(const BleCommand& cmd, uint32_t timeout_ms = 1000);

    // Convenience methods for common operations
    void enable(bool enabled);
    void startAdvertising();
    void stopAdvertising();
    void setAutoAdvertising(bool enabled);
    void setDeviceName(const std::string& name);
    void setMaxConnections(uint8_t max_connections);
    void disconnectAll();
    void disconnect(uint16_t conn_handle);
    void forgetPeer(const NimBLEAddress& address, std::function<void(bool success)> callback = nullptr);
    void startDirectedAdvertising(const NimBLEAddress& address, uint32_t timeout_seconds = 15);

    // HID convenience methods
    bool sendKey(uint8_t keycode, uint8_t modifier = 0, BleHidTarget target = BleHidTarget::ALL, const std::string& specific_mac = "");
    bool sendText(const std::string& text, BleHidTarget target = BleHidTarget::ALL, const std::string& specific_mac = "");
    bool sendMouseMove(int8_t dx, int8_t dy, int8_t wheel = 0, uint8_t buttons = 0, BleHidTarget target = BleHidTarget::ALL, const std::string& specific_mac = "");
    bool mouseClick(uint8_t buttons, BleHidTarget target = BleHidTarget::ALL, const std::string& specific_mac = "");

    // Client/Central convenience methods
    void startScan(uint32_t duration_ms = 5000);
    void stopScan();
    void connectToDevice(const NimBLEAddress& address);
    void disconnectFromDevice();
    void clearScanResults();

private:
    BleManager();
    ~BleManager();
    BleManager(const BleManager&) = delete;
    BleManager& operator=(const BleManager&) = delete;

    static void bleTask(void* pvParameters);
    void processCommand(const BleCommand& cmd);

    TaskHandle_t task_handle_ = nullptr;
    QueueHandle_t command_queue_ = nullptr;
    static constexpr size_t QUEUE_LENGTH = 10;
};

#endif // BLE_MANAGER_H

#pragma once

#include <NimBLEDevice.h>
#include <string>
#include <vector>
#include <functional>

class BleManager;  // Forward declaration

/**
 * @struct ScanResult
 * @brief Represents a discovered BLE device during scanning
 */
struct ScanResult {
    NimBLEAddress address;
    std::string name;
    int rssi;
    bool hasName() const { return !name.empty(); }
};

/**
 * @class BleClientManager
 * @brief Manages BLE Central/Client role functionality
 *
 * This class handles scanning for BLE devices and connecting to them as a client.
 * It works in parallel with BleHidManager (Peripheral/Server role).
 *
 * NOTE: This class should not be accessed directly from the UI thread.
 * Use BleManager to post commands instead.
 */
class BleClientManager {
public:
    static BleClientManager& getInstance();
    ~BleClientManager() = default;

    // Read-only status methods (thread-safe)
    bool isScanning() const { return is_scanning_; }
    bool isClientConnected() const { return client_connected_; }
    std::vector<ScanResult> getScanResults() const;
    size_t getScanResultCount() const { return scan_results_.size(); }

    // Callback type for scan results
    using ScanCallback = std::function<void(const ScanResult& result)>;
    void setScanCallback(ScanCallback callback) { scan_callback_ = callback; }

private:
    BleClientManager() = default;
    BleClientManager(const BleClientManager&) = delete;
    BleClientManager& operator=(const BleClientManager&) = delete;

    // BleManager and scan callbacks need access to private methods
    friend class BleManager;
    friend class BleClientScanCallbacks;

    // Control methods - private, only accessible via BleManager
    bool init();
    bool startScan(uint32_t duration_ms = 5000);
    void stopScan();
    bool connectTo(const NimBLEAddress& address);
    void disconnectClient();
    void clearScanResults();

    // Internal callbacks
    void handleScanResult(NimBLEAdvertisedDevice* device);
    void handleScanComplete();
    static void scanCompleteCallback(NimBLEScanResults results);

    // State
    bool initialized_ = false;
    bool is_scanning_ = false;
    bool client_connected_ = false;
    std::vector<ScanResult> scan_results_;
    ScanCallback scan_callback_;

    NimBLEClient* client_ = nullptr;
    NimBLEAddress connected_address_;

    // Scan settings
    static constexpr uint8_t MAX_SCAN_RESULTS = 20;
};

/**
 * @class BleClientScanCallbacks
 * @brief Callbacks for BLE scanning operations
 */
class BleClientScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
    explicit BleClientScanCallbacks(BleClientManager& manager) : manager_(manager) {}

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        manager_.handleScanResult(advertisedDevice);
    }

private:
    BleClientManager& manager_;
};

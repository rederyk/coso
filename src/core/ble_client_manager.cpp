#include "ble_client_manager.h"
#include "utils/logger.h"
#include <algorithm>

BleClientManager& BleClientManager::getInstance() {
    static BleClientManager instance;
    return instance;
}

BleClientManager::~BleClientManager() {
    // Clean up scan callbacks
    if (scan_callbacks_) {
        delete scan_callbacks_;
        scan_callbacks_ = nullptr;
    }
}

bool BleClientManager::init() {
    if (initialized_) {
        return true;
    }

    Logger::getInstance().info("[BleClient] Initializing BLE Client Manager");

    // NimBLE should already be initialized by BleHidManager
    // Create scan callbacks once (reused across all scans)
    if (!scan_callbacks_) {
        scan_callbacks_ = new BleClientScanCallbacks(*this);
    }

    initialized_ = true;

    Logger::getInstance().info("[BleClient] Client Manager initialized");
    return true;
}

bool BleClientManager::startScan(uint32_t duration_ms) {
    if (!initialized_) {
        Logger::getInstance().error("[BleClient] Not initialized");
        return false;
    }

    if (is_scanning_) {
        Logger::getInstance().warn("[BleClient] Scan already in progress");
        return false;
    }

    // Clear previous results
    scan_results_.clear();

    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (!pScan) {
        Logger::getInstance().error("[BleClient] Failed to get scan object");
        return false;
    }

    // Set scan callbacks (reuse the same callback object)
    if (scan_callbacks_) {
        pScan->setAdvertisedDeviceCallbacks(scan_callbacks_);
    } else {
        Logger::getInstance().error("[BleClient] Scan callbacks not initialized");
        return false;
    }

    // Configure scan parameters
    pScan->setActiveScan(true);     // Active scan uses more power, but gets all data
    pScan->setInterval(100);        // Scan interval in ms
    pScan->setWindow(99);           // Scan window in ms
    pScan->setDuplicateFilter(true); // Filter duplicates

    // Start scan
    uint32_t duration_sec = duration_ms / 1000;
    if (duration_sec == 0) {
        duration_sec = 5;  // Default 5 seconds
    }

    Logger::getInstance().infof("[BleClient] Starting scan for %u seconds", duration_sec);
    is_scanning_ = true;

    // Start async scan (non-blocking)
    // Note: NimBLE will call the static callback when scan is complete
    pScan->start(duration_sec, &BleClientManager::scanCompleteCallback, false);

    return true;
}

// Static callback for scan complete
void BleClientManager::scanCompleteCallback(NimBLEScanResults results) {
    (void)results;
    getInstance().handleScanComplete();
}

void BleClientManager::stopScan() {
    if (!is_scanning_) {
        return;
    }

    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan) {
        pScan->stop();
    }

    is_scanning_ = false;
    Logger::getInstance().info("[BleClient] Scan stopped");
}

void BleClientManager::handleScanResult(NimBLEAdvertisedDevice* device) {
    if (!device) return;

    // Check if we already have this device
    NimBLEAddress addr = device->getAddress();
    auto it = std::find_if(scan_results_.begin(), scan_results_.end(),
        [&addr](const ScanResult& r) { return r.address == addr; });

    if (it != scan_results_.end()) {
        // Update existing entry with new RSSI
        it->rssi = device->getRSSI();
        if (device->haveName() && it->name.empty()) {
            it->name = device->getName();
        }
    } else {
        // Add new device if we have space
        if (scan_results_.size() < MAX_SCAN_RESULTS) {
            ScanResult result;
            result.address = addr;
            result.name = device->haveName() ? device->getName() : "";
            result.rssi = device->getRSSI();
            scan_results_.push_back(result);

            Logger::getInstance().infof("[BleClient] Found device: %s (%s) RSSI: %d",
                result.address.toString().c_str(),
                result.name.empty() ? "Unknown" : result.name.c_str(),
                result.rssi);

            // Call callback if set
            if (scan_callback_) {
                scan_callback_(result);
            }
        }
    }
}

void BleClientManager::handleScanComplete() {
    is_scanning_ = false;
    Logger::getInstance().infof("[BleClient] Scan complete. Found %d devices", scan_results_.size());
}

std::vector<ScanResult> BleClientManager::getScanResults() const {
    return scan_results_;
}

void BleClientManager::clearScanResults() {
    scan_results_.clear();
    Logger::getInstance().info("[BleClient] Scan results cleared");
}

bool BleClientManager::connectTo(const NimBLEAddress& address) {
    if (!initialized_) {
        Logger::getInstance().error("[BleClient] Not initialized");
        return false;
    }

    if (client_connected_) {
        Logger::getInstance().warn("[BleClient] Already connected to a device");
        return false;
    }

    Logger::getInstance().infof("[BleClient] Connecting to %s", address.toString().c_str());

    // Create client if needed
    if (!client_) {
        client_ = NimBLEDevice::createClient();
        if (!client_) {
            Logger::getInstance().error("[BleClient] Failed to create client");
            return false;
        }
    }

    // Attempt connection
    if (!client_->connect(address)) {
        Logger::getInstance().errorf("[BleClient] Failed to connect to %s", address.toString().c_str());
        return false;
    }

    client_connected_ = true;
    connected_address_ = address;
    Logger::getInstance().infof("[BleClient] Connected to %s", address.toString().c_str());

    // TODO: Discover services and characteristics here if needed

    return true;
}

void BleClientManager::disconnectClient() {
    if (!client_connected_ || !client_) {
        return;
    }

    Logger::getInstance().infof("[BleClient] Disconnecting from %s", connected_address_.toString().c_str());

    client_->disconnect();
    client_connected_ = false;
    connected_address_ = NimBLEAddress();

    Logger::getInstance().info("[BleClient] Disconnected");
}

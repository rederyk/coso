#include "ble_manager.h"
#include "utils/logger.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"

// Constructor
BleManager::BleManager() {
}

// Destructor
BleManager::~BleManager() {
}

// Initialize the BleManager
void BleManager::init() {
}

// Start the BLE task
void BleManager::start() {
    xTaskCreatePinnedToCore(
        this->ble_task,    // Task function
        "ble_task",        // Name of the task
        4096,              // Stack size in words
        NULL,              // Task input parameter
        1,                 // Priority of the task
        NULL,              // Task handle
        0                  // Core where the task should run
    );
    log_i("BLE task started on core 0");
}

// Placeholder for BLE server callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      log_i("BLE client connected");
    };
    void onDisconnect(BLEServer* pServer) {
      log_i("BLE client disconnected");
    }
};

// The FreeRTOS task for handling BLE
void BleManager::ble_task(void *pvParameters) {
    log_i("BLE task running");

    // Create the BLE Device
    BLEDevice::init("ESP32-S3"); // Device name

    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    // Using the UUID from the example sketch
    BLEService *pService = pServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");

    // Create a BLE Characteristic
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

    pCharacteristic->setValue("Hello World");
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    log_i("BLE advertising started");

    for (;;) {
        // Keep the task alive
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

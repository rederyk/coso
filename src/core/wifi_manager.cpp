#include "wifi_manager.h"
#include "settings_manager.h"
#include "utils/logger.h"

// Constructor
WifiManager::WifiManager() {
    // Initialization, if any, can go here
}

// Destructor
WifiManager::~WifiManager() {
    // Cleanup, if any, can go here
}

// Initialize the WifiManager
void WifiManager::init() {
    // Nothing to do here for now
    // We could pre-load settings if needed
}

// Start the WiFi task
void WifiManager::start() {
    xTaskCreatePinnedToCore(
        this->wifi_task,    // Task function
        "wifi_task",        // Name of the task
        4096,               // Stack size in words
        NULL,               // Task input parameter
        1,                  // Priority of the task
        NULL,               // Task handle
        0                   // Core where the task should run
    );
    log_i("WiFi task started on core 0");
}

// The FreeRTOS task for handling WiFi
void WifiManager::wifi_task(void *pvParameters) {
    log_i("WiFi task running");

    // Connect to WiFi from settings
    const std::string& ssid = SettingsManager::getInstance().getWifiSsid();
    const std::string& password = SettingsManager::getInstance().getWifiPassword();

    if (!ssid.empty()) {
        WiFi.begin(ssid.c_str(), password.c_str());
        log_i("Connecting to WiFi: %s", ssid.c_str());

        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 30) {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            log_d(".");
            retries++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            log_i("WiFi connected, IP address: %s", WiFi.localIP().toString().c_str());
        } else {
            log_e("Failed to connect to WiFi");
        }
    } else {
        log_w("WiFi SSID not configured.");
    }

    // Task loop
    for (;;) {
        // Keep the task alive
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

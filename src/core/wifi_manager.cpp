#include "wifi_manager.h"
#include "settings_manager.h"
#include "core/time_manager.h"
#include "utils/logger.h"
#include "drivers/rgb_led_driver.h"
#include "core/task_config.h"
#include "core/system_tasks.h"
#include "core/web_server_manager.h"
#include "core/web_data_manager.h"

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
        TaskConfig::STACK_WIFI, // Stack size in words
        NULL,               // Task input parameter
        TaskConfig::PRIO_WIFI,  // Priority of the task
        NULL,               // Task handle
        TaskConfig::CORE_WORK   // Core where the task should run
    );
    log_i("WiFi task started on core %d", static_cast<int>(TaskConfig::CORE_WORK));
}

// The FreeRTOS task for handling WiFi
void WifiManager::wifi_task(void *pvParameters) {
    log_i("WiFi task running");

    RgbLedManager& rgb_led = RgbLedManager::getInstance();

    // Connect to WiFi from settings
    const std::string& ssid = SettingsManager::getInstance().getWifiSsid();
    const std::string& password = SettingsManager::getInstance().getWifiPassword();

    if (!ssid.empty()) {
        // Imposta LED su "connecting"
        if (rgb_led.isInitialized()) {
            rgb_led.setState(RgbLedManager::LedState::WIFI_CONNECTING);
        }

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
            // Imposta LED su "connected"
            if (rgb_led.isInitialized()) {
                rgb_led.setState(RgbLedManager::LedState::WIFI_CONNECTED);
            }

            WebDataManager::getInstance().notifyWifiReady();

            // Initialize and sync time via NTP
            auto& settings = SettingsManager::getInstance();
            auto& time_mgr = TimeManager::getInstance();

            time_mgr.setTimezone(settings.getTimezone());
            time_mgr.setNtpServers(
                settings.getNtpServer(),
                settings.getNtpServer2(),
                settings.getNtpServer3()
            );
            time_mgr.setAutoSync(
                settings.getAutoTimeSync(),
                settings.getTimeSyncIntervalHours()
            );

            if (time_mgr.begin()) {
                log_i("Time synchronized via NTP");
            } else {
                log_w("Time sync failed, will retry automatically");
            }
            // Avvia il server web quando la rete è pronta
            WebServerManager& web = WebServerManager::getInstance();
            if (!web.isRunning()) {
                web.start();
            }
            // Notifica la UI del cambio di stato
            UiMessage msg{};
            msg.type = UiMessageType::WifiStatus;
            msg.value = 1; // Connected
            SystemTasks::postUiMessage(msg, 0);
        } else {
            log_e("Failed to connect to WiFi");
            // Imposta LED su "error"
            if (rgb_led.isInitialized()) {
                rgb_led.setState(RgbLedManager::LedState::WIFI_ERROR);
            }

            WebDataManager::getInstance().notifyWifiDisconnected();
            // Notifica la UI dell'errore
            UiMessage msg{};
            msg.type = UiMessageType::WifiStatus;
            msg.value = 0; // Error/Disconnected
            SystemTasks::postUiMessage(msg, 0);
        }
    } else {
        log_w("WiFi SSID not configured.");
        // Non spegnere il LED, lascia che il BLE lo controlli
    }

    // Task loop - monitora lo stato della connessione
    wl_status_t last_status = WiFi.status();
    for (;;) {
        wl_status_t current_status = WiFi.status();

        // Rileva cambio di stato
        if (current_status != last_status) {
            if (current_status == WL_CONNECTED) {
                log_i("WiFi reconnected");
                if (rgb_led.isInitialized()) {
                    rgb_led.setState(RgbLedManager::LedState::WIFI_CONNECTED);
                }

                WebDataManager::getInstance().notifyWifiReady();

                // Re-sync time after reconnection
                auto& time_mgr = TimeManager::getInstance();
                if (!time_mgr.isInitialized()) {
                    auto& settings = SettingsManager::getInstance();
                    time_mgr.setTimezone(settings.getTimezone());
                    time_mgr.setNtpServers(
                        settings.getNtpServer(),
                        settings.getNtpServer2(),
                        settings.getNtpServer3()
                    );
                    time_mgr.begin();
                } else {
                    time_mgr.syncNow(5000);  // Quick resync
                }

                WebServerManager& web = WebServerManager::getInstance();
                if (!web.isRunning()) {
                    web.start();
                }
                // Notifica la UI della riconnessione
                UiMessage msg{};
                msg.type = UiMessageType::WifiStatus;
                msg.value = 1; // Connected
                SystemTasks::postUiMessage(msg, 0);
            } else {
                log_w("WiFi disconnected");
                // Notifica la UI della disconnessione
                UiMessage msg{};
                msg.type = UiMessageType::WifiStatus;
                msg.value = 0; // Disconnected
                SystemTasks::postUiMessage(msg, 0);
                WebDataManager::getInstance().notifyWifiDisconnected();
            }
            last_status = current_status;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

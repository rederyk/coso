# Estensioni Architettura OS ESP32 - Service Layer & Dashboard Custom

> **⚠️ DOCUMENTO DEPRECATO**
>
> Questo documento è stato **integrato** in:
> - **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** - Architettura completa unificata
> - **[ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md)** - Gestione periferiche hardware
>
> **Usa i nuovi documenti per l'implementazione.** Questo file è mantenuto solo per riferimento storico.

---

## Executive Summary

Questo documento integra il [REPORT_ARCHITETTURA_OS_ESP32.md](REPORT_ARCHITETTURA_OS_ESP32.md) con le estensioni necessarie per supportare:

- ✅ **Servizi di Sistema** (WiFi, BLE, Network)
- ✅ **Dashboard Customizzabile** con widget
- ✅ **Background Tasks** thread-safe
- ✅ **Event-driven Architecture** scalabile

**📌 IMPORTANTE**: Questi contenuti sono stati **unificati** nel documento [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) insieme a:
- Peripheral Manager Layer (GPIO, I2C, SPI, ADC, PWM, UART)
- Architettura completa e coerente
- Esempi di codice ready-to-use
- Checklist pre-deployment

**👉 Leggi [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) per la documentazione completa e aggiornata.**

---

Basato su:
- Analisi architettura **Tactility OS** (ESP32 OS with app support)
- Best practices **LVGL + FreeRTOS** dal port ufficiale
- Problemi comuni risolti dalla community ESP32/LVGL
- Pattern da **ESP32 Lifecycle Manager**

---

## 1. Problemi Comuni Identificati dalla Community

### 1.1 Conflitti WiFi/BLE/LVGL

**PROBLEMA CRITICO TROVATO:**

> "When any 2 of BLE/WiFi/LVGL are run together, they work flawlessly, but all 3 together can cause issues"
>
> Fonte: ESP32 Forum - [WiFi + BLE not playing nicely with LVGL](https://esp32.com/viewtopic.php?t=24551)

**Cause:**

- il topic della fonte e del 2021 e molto probabilmete un problema di ram, 
esp32-s3 probabilmente non avra questo problema ma, ottimizzare la ram e sempre bene accetto

- WiFi stack tasks girano **SOLO su Core 0** (PRO CPU)
- LVGL può monopolizzare un core se non gestito correttamente
- Cache misses e SPI arbitration rallentano entrambi i core
- Mancanza di sincronizzazione causa race conditions

**SOLUZIONE VERIFICATA:**

```cpp
// WiFi/BLE tasks → Core 0 (PRO CPU)
// LVGL GUI task → Core 1 (APP CPU)

xTaskCreatePinnedToCore(
    wifi_task,      // Task function
    "wifi_service", // Name
    4096,           // Stack size
    NULL,           // Parameters
    5,              // Priority (più alta di LVGL)
    NULL,           // Task handle
    0               // Core 0 (PRO CPU)
);

xTaskCreatePinnedToCore(
    gui_task,       // Task function
    "lvgl_gui",     // Name
    8192,           // Stack size (LVGL richiede più memoria)
    NULL,           // Parameters
    3,              // Priority (più bassa di WiFi)
    NULL,           // Task handle
    1               // Core 1 (APP CPU)
);
```

**Priorità Raccomandate:**
- WiFi/BLE Service: `5-7` (alta, per responsività network)
- Background Services: `3-4` (media)
- LVGL GUI: `2-3` (media-bassa, può aspettare)
- Idle/Utility: `0-1` (bassa)

---

### 1.2 Thread Safety con LVGL

**PROBLEMA:**

> "You need a mutex which should be taken before calling lv_task_handler and released after it. And you must use that mutex in other tasks around every LVGL related code."
>
> Fonte: [LVGL Official ESP32 Port](https://github.com/lvgl/lv_port_esp32)

**SOLUZIONE - Pattern Ufficiale LVGL:**

```cpp
// Global mutex per LVGL
SemaphoreHandle_t lvgl_mutex = NULL;

// Inizializzazione (in app_main o setup)
void lvgl_init() {
    lv_init();

    // Crea mutex GLOBALE
    lvgl_mutex = xSemaphoreCreateMutex();

    // Setup display, touch, etc...

    // Crea task LVGL su Core 1
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 3, NULL, 1);
}

// Task LVGL dedicato
static void lvgl_task(void* pvParameter) {
    while (1) {
        // Delay standard 10ms
        vTaskDelay(pdMS_TO_TICKS(10));

        // LOCK prima di lv_task_handler
        if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(lvgl_mutex);
        }
    }
}

// Da QUALSIASI altro task (es. WiFi service)
void wifi_update_gui(const char* status) {
    // SEMPRE LOCK prima di chiamare funzioni LVGL
    if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
        lv_label_set_text(wifi_status_label, status);
        xSemaphoreGive(lvgl_mutex);
    }
}
```

**Timer Tick Separato:**

```cpp
// Timer ad alta precisione per LVGL tick (1ms)
static void lv_tick_task(void *arg) {
    lv_tick_inc(1);  // Incrementa tick interno LVGL
}

void setup_lvgl_tick() {
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000)); // 1ms
}
```

---

### 1.3 Memory Management

**PROBLEMA:**

> "lv_task_handler stops running without crash after a few screens"
>
> Fonte: [LVGL Forum - lv_task_handler stops](https://forum.lvgl.io/t/lv-task-handler-stops-running-without-crash/2984)

**Cause Comuni:**
- Stack overflow dei task FreeRTOS
- Heap fragmentation (soprattutto con molti widget)
- Memory leak in screen transitions

**SOLUZIONE:**

```cpp
// Aumenta stack task LVGL
xTaskCreatePinnedToCore(lvgl_task, "lvgl",
    8192,  // Minimo 8KB per LVGL con GUI complesse
    NULL, 3, NULL, 1);

// Usa PSRAM per buffer LVGL (ESP32-S3 ha PSRAM!)
#define LVGL_BUFFER_SIZE (LV_HOR_RES_MAX * LV_VER_RES_MAX / 10)
static lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(
    LVGL_BUFFER_SIZE * sizeof(lv_color_t),
    MALLOC_CAP_SPIRAM  // Usa PSRAM invece di SRAM interna
);

// Cleanup aggressivo nelle screen transitions
void ScreenManager::replaceScreen(Screen* new_screen) {
    if (current_screen) {
        current_screen->onDestroy();
        delete current_screen;  // Libera memoria PRIMA di caricare nuova screen
        current_screen = nullptr;
    }

    // Force garbage collection LVGL
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("LVGL Mem: %d%% used\n", mon.used_pct);

    current_screen = new_screen;
    // ...
}
```

---

## 2. Architettura Service Layer - Ispirato da Tactility OS

### 2.1 Pattern Service Manager

Tactility OS usa:
- **ELF loading** per app dinamiche (troppo complesso per il nostro caso)
- **App manifest** simile ad Android (idea buona!)
- **Service lifecycle** con start/stop/state

**Adattamento per il nostro OS:**

```cpp
// service_base.h
#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum ServiceState {
    SERVICE_STOPPED,
    SERVICE_STARTING,
    SERVICE_RUNNING,
    SERVICE_STOPPING,
    SERVICE_ERROR
};

class SystemService {
protected:
    TaskHandle_t service_task = nullptr;
    ServiceState state = SERVICE_STOPPED;
    const char* service_name = "Unknown";
    uint32_t stack_size = 4096;
    UBaseType_t priority = 4;
    BaseType_t core_id = 0;  // Default Core 0

    // Override questi metodi
    virtual void serviceMain() = 0;  // Loop principale del servizio
    virtual bool onStart() { return true; }  // Inizializzazione
    virtual void onStop() {}  // Cleanup

private:
    static void taskWrapper(void* pvParameters) {
        SystemService* service = static_cast<SystemService*>(pvParameters);
        service->serviceMain();
        vTaskDelete(nullptr);
    }

public:
    virtual ~SystemService() {
        stop();
    }

    bool start() {
        if (state == SERVICE_RUNNING) {
            Serial.printf("[%s] Already running\n", service_name);
            return true;
        }

        state = SERVICE_STARTING;
        Serial.printf("[%s] Starting on core %d...\n", service_name, core_id);

        if (!onStart()) {
            state = SERVICE_ERROR;
            return false;
        }

        BaseType_t result = xTaskCreatePinnedToCore(
            taskWrapper,
            service_name,
            stack_size,
            this,
            priority,
            &service_task,
            core_id
        );

        if (result != pdPASS) {
            state = SERVICE_ERROR;
            Serial.printf("[%s] Failed to create task\n", service_name);
            return false;
        }

        state = SERVICE_RUNNING;
        Serial.printf("[%s] Started successfully\n", service_name);
        return true;
    }

    void stop() {
        if (state != SERVICE_RUNNING) return;

        state = SERVICE_STOPPING;
        onStop();

        if (service_task != nullptr) {
            vTaskDelete(service_task);
            service_task = nullptr;
        }

        state = SERVICE_STOPPED;
        Serial.printf("[%s] Stopped\n", service_name);
    }

    ServiceState getState() const { return state; }
    const char* getName() const { return service_name; }
    bool isRunning() const { return state == SERVICE_RUNNING; }
};
```

---

### 2.2 WiFi Service - Implementazione Completa

```cpp
// wifi_service.h
#pragma once
#include "service_base.h"
#include <WiFi.h>
#include "event_router.h"

class WiFiService : public SystemService {
public:
    enum WiFiState {
        WIFI_DISCONNECTED,
        WIFI_CONNECTING,
        WIFI_CONNECTED,
        WIFI_ERROR
    };

private:
    WiFiState wifi_state = WIFI_DISCONNECTED;
    String ssid;
    String password;
    IPAddress ip_address;
    bool auto_reconnect = true;
    uint32_t reconnect_delay_ms = 5000;

    // EventRouter per notifiche
    EventRouter* event_router = nullptr;

public:
    WiFiService() {
        service_name = "WiFiService";
        stack_size = 4096;
        priority = 5;  // Alta priorità
        core_id = 0;   // Core 0 (OBBLIGATORIO per WiFi ESP32)
        event_router = EventRouter::getInstance();
    }

    void configure(const char* wifi_ssid, const char* wifi_password) {
        ssid = wifi_ssid;
        password = wifi_password;
    }

    void connect() {
        if (wifi_state == WIFI_CONNECTING || wifi_state == WIFI_CONNECTED) {
            return;
        }

        wifi_state = WIFI_CONNECTING;
        Serial.printf("[WiFi] Connecting to %s...\n", ssid.c_str());

        WiFi.begin(ssid.c_str(), password.c_str());

        // Notifica UI
        event_router->publish("wifi.connecting", (void*)ssid.c_str());
    }

    void disconnect() {
        WiFi.disconnect();
        wifi_state = WIFI_DISCONNECTED;
        event_router->publish("wifi.disconnected", nullptr);
    }

    WiFiState getWiFiState() const { return wifi_state; }
    IPAddress getIP() const { return ip_address; }
    int getRSSI() const { return WiFi.RSSI(); }

protected:
    bool onStart() override {
        WiFi.mode(WIFI_STA);
        return true;
    }

    void onStop() override {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }

    void serviceMain() override {
        while (true) {
            switch (wifi_state) {
                case WIFI_CONNECTING: {
                    if (WiFi.status() == WL_CONNECTED) {
                        wifi_state = WIFI_CONNECTED;
                        ip_address = WiFi.localIP();

                        Serial.printf("[WiFi] Connected! IP: %s\n",
                                      ip_address.toString().c_str());

                        // Notifica tutti i subscriber
                        event_router->publish("wifi.connected",
                                              (void*)ip_address.toString().c_str());
                    } else if (WiFi.status() == WL_CONNECT_FAILED) {
                        wifi_state = WIFI_ERROR;
                        Serial.println("[WiFi] Connection failed!");
                        event_router->publish("wifi.error", (void*)"Connection failed");
                    }
                    break;
                }

                case WIFI_CONNECTED: {
                    if (WiFi.status() != WL_CONNECTED) {
                        wifi_state = WIFI_DISCONNECTED;
                        Serial.println("[WiFi] Disconnected!");
                        event_router->publish("wifi.disconnected", nullptr);

                        // Auto-reconnect
                        if (auto_reconnect) {
                            vTaskDelay(pdMS_TO_TICKS(reconnect_delay_ms));
                            connect();
                        }
                    }
                    break;
                }

                case WIFI_ERROR: {
                    if (auto_reconnect) {
                        vTaskDelay(pdMS_TO_TICKS(reconnect_delay_ms));
                        wifi_state = WIFI_DISCONNECTED;
                        connect();
                    }
                    break;
                }

                default:
                    break;
            }

            // Check ogni 500ms
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
};
```

---

### 2.3 BLE Service - Implementazione

```cpp
// ble_service.h
#pragma once
#include "service_base.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "event_router.h"

class BLEService : public SystemService {
private:
    BLEServer* server = nullptr;
    BLEAdvertising* advertising = nullptr;
    bool device_connected = false;
    String device_name;
    EventRouter* event_router = nullptr;

    class ServerCallbacks : public BLEServerCallbacks {
    private:
        BLEService* parent;
    public:
        ServerCallbacks(BLEService* p) : parent(p) {}

        void onConnect(BLEServer* pServer) override {
            parent->device_connected = true;
            parent->event_router->publish("ble.connected", nullptr);
            Serial.println("[BLE] Device connected");
        }

        void onDisconnect(BLEServer* pServer) override {
            parent->device_connected = false;
            parent->event_router->publish("ble.disconnected", nullptr);
            Serial.println("[BLE] Device disconnected");

            // Restart advertising
            parent->advertising->start();
        }
    };

public:
    BLEService(const char* name = "ESP32_OS") {
        service_name = "BLEService";
        device_name = name;
        stack_size = 8192;  // BLE richiede più stack
        priority = 5;
        core_id = 0;  // Core 0 (come WiFi)
        event_router = EventRouter::getInstance();
    }

    void startAdvertising() {
        if (advertising) {
            advertising->start();
            Serial.println("[BLE] Advertising started");
        }
    }

    void stopAdvertising() {
        if (advertising) {
            advertising->stop();
            Serial.println("[BLE] Advertising stopped");
        }
    }

    bool isConnected() const { return device_connected; }
    BLEServer* getServer() { return server; }

protected:
    bool onStart() override {
        BLEDevice::init(device_name.c_str());

        server = BLEDevice::createServer();
        server->setCallbacks(new ServerCallbacks(this));

        advertising = BLEDevice::getAdvertising();

        Serial.printf("[BLE] Initialized as '%s'\n", device_name.c_str());
        return true;
    }

    void onStop() override {
        if (advertising) {
            advertising->stop();
        }
        if (server) {
            // Cleanup BLE
        }
        BLEDevice::deinit(false);
    }

    void serviceMain() override {
        // BLE è event-driven, non serve loop attivo
        // I callback gestiscono tutto
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));

            // Qui puoi aggiungere logica periodica
            // es. pubblicare statistiche
            if (device_connected) {
                event_router->publish("ble.heartbeat", nullptr);
            }
        }
    }
};
```

---

### 2.4 Service Manager - Registro Centralizzato

```cpp
// service_manager.h
#pragma once
#include <map>
#include <vector>
#include "service_base.h"

class ServiceManager {
private:
    static ServiceManager* instance;
    std::map<const char*, SystemService*> services;

    ServiceManager() {}

public:
    static ServiceManager* getInstance() {
        if (instance == nullptr) {
            instance = new ServiceManager();
        }
        return instance;
    }

    void registerService(const char* name, SystemService* service) {
        if (services.find(name) != services.end()) {
            Serial.printf("[ServiceMgr] Service '%s' already registered\n", name);
            return;
        }

        services[name] = service;
        Serial.printf("[ServiceMgr] Registered service: %s\n", name);
    }

    SystemService* getService(const char* name) {
        auto it = services.find(name);
        if (it != services.end()) {
            return it->second;
        }
        return nullptr;
    }

    void startAll() {
        Serial.println("[ServiceMgr] Starting all services...");
        for (auto& pair : services) {
            pair.second->start();
        }
    }

    void stopAll() {
        Serial.println("[ServiceMgr] Stopping all services...");
        for (auto& pair : services) {
            pair.second->stop();
        }
    }

    void startService(const char* name) {
        auto service = getService(name);
        if (service) {
            service->start();
        }
    }

    void stopService(const char* name) {
        auto service = getService(name);
        if (service) {
            service->stop();
        }
    }

    std::vector<SystemService*> getRunningServices() {
        std::vector<SystemService*> running;
        for (auto& pair : services) {
            if (pair.second->isRunning()) {
                running.push_back(pair.second);
            }
        }
        return running;
    }

    void printStatus() {
        Serial.println("\n=== Service Manager Status ===");
        for (auto& pair : services) {
            ServiceState state = pair.second->getState();
            const char* state_str =
                state == SERVICE_RUNNING ? "RUNNING" :
                state == SERVICE_STOPPED ? "STOPPED" :
                state == SERVICE_STARTING ? "STARTING" :
                state == SERVICE_STOPPING ? "STOPPING" : "ERROR";

            Serial.printf("  [%s] %s\n", pair.first, state_str);
        }
        Serial.println("==============================\n");
    }
};

ServiceManager* ServiceManager::instance = nullptr;
```

---

## 3. Dashboard Widget System

### 3.1 Widget Base Class

```cpp
// dashboard_widget.h
#pragma once
#include <lvgl.h>
#include "event_router.h"

class DashboardWidget {
protected:
    lv_obj_t* container = nullptr;
    uint8_t span_cols = 1;  // Quante colonne occupa nel grid
    uint8_t span_rows = 1;  // Quante righe occupa
    bool visible = true;
    EventRouter* event_router = nullptr;

    // Mutex LVGL globale (condiviso con app)
    extern SemaphoreHandle_t lvgl_mutex;

public:
    DashboardWidget() {
        event_router = EventRouter::getInstance();
    }

    virtual ~DashboardWidget() {
        if (container) {
            lv_obj_del(container);
        }
    }

    // Override questi metodi
    virtual void create(lv_obj_t* parent) = 0;
    virtual void update() = 0;  // Chiamato periodicamente
    virtual void onEvent(const char* event, void* data) {}  // Per EventRouter

    // Helpers
    uint8_t getColSpan() const { return span_cols; }
    uint8_t getRowSpan() const { return span_rows; }
    lv_obj_t* getContainer() { return container; }

    void show() {
        if (container && !visible) {
            lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
            visible = true;
        }
    }

    void hide() {
        if (container && visible) {
            lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
            visible = false;
        }
    }

    // Thread-safe update helper
    void safeUpdateUI(std::function<void()> updateFunc) {
        if (pdTRUE == xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100))) {
            updateFunc();
            xSemaphoreGive(lvgl_mutex);
        }
    }
};
```

---

### 3.2 WiFi Status Widget

```cpp
// widgets/wifi_status_widget.h
#pragma once
#include "dashboard_widget.h"
#include "wifi_service.h"

class WiFiStatusWidget : public DashboardWidget {
private:
    lv_obj_t* icon_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* ip_label = nullptr;
    lv_obj_t* rssi_bar = nullptr;

    WiFiService* wifi_service = nullptr;

public:
    WiFiStatusWidget() {
        span_cols = 2;  // Più largo
        span_rows = 1;

        wifi_service = (WiFiService*)ServiceManager::getInstance()
                       ->getService("wifi");

        // Subscribe agli eventi WiFi
        event_router->subscribe("wifi.connected", [this](const char* e, void* d) {
            onEvent(e, d);
        });
        event_router->subscribe("wifi.disconnected", [this](const char* e, void* d) {
            onEvent(e, d);
        });
        event_router->subscribe("wifi.connecting", [this](const char* e, void* d) {
            onEvent(e, d);
        });
    }

    void create(lv_obj_t* parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, LV_PCT(100), 80);
        lv_obj_set_style_radius(container, 10, 0);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(container, 10, 0);

        // Icona WiFi
        icon_label = lv_label_create(container);
        lv_label_set_text(icon_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);

        // Container info
        lv_obj_t* info_container = lv_obj_create(container);
        lv_obj_set_flex_grow(info_container, 1);
        lv_obj_set_style_bg_opa(info_container, 0, 0);
        lv_obj_set_style_border_width(info_container, 0, 0);
        lv_obj_set_style_pad_all(info_container, 0, 0);
        lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_COLUMN);

        // Status label
        status_label = lv_label_create(info_container);
        lv_label_set_text(status_label, "Disconnected");
        lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);

        // IP label
        ip_label = lv_label_create(info_container);
        lv_label_set_text(ip_label, "---");
        lv_obj_set_style_text_color(ip_label, lv_color_hex(0xBDC3C7), 0);
        lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_12, 0);

        // RSSI bar
        rssi_bar = lv_bar_create(info_container);
        lv_obj_set_size(rssi_bar, LV_PCT(100), 8);
        lv_bar_set_range(rssi_bar, -100, -30);
        lv_bar_set_value(rssi_bar, -100, LV_ANIM_OFF);

        update();
    }

    void update() override {
        if (!wifi_service) return;

        safeUpdateUI([this]() {
            auto state = wifi_service->getWiFiState();

            switch (state) {
                case WiFiService::WIFI_CONNECTED: {
                    lv_label_set_text(status_label, "Connected");
                    lv_obj_set_style_text_color(status_label,
                                                 lv_color_hex(0x2ECC71), 0);

                    IPAddress ip = wifi_service->getIP();
                    lv_label_set_text_fmt(ip_label, "%s", ip.toString().c_str());

                    int rssi = wifi_service->getRSSI();
                    lv_bar_set_value(rssi_bar, rssi, LV_ANIM_ON);

                    // Colore barra basato su segnale
                    if (rssi > -50) {
                        lv_obj_set_style_bg_color(rssi_bar,
                                                   lv_color_hex(0x2ECC71),
                                                   LV_PART_INDICATOR);
                    } else if (rssi > -70) {
                        lv_obj_set_style_bg_color(rssi_bar,
                                                   lv_color_hex(0xF39C12),
                                                   LV_PART_INDICATOR);
                    } else {
                        lv_obj_set_style_bg_color(rssi_bar,
                                                   lv_color_hex(0xE74C3C),
                                                   LV_PART_INDICATOR);
                    }
                    break;
                }

                case WiFiService::WIFI_CONNECTING:
                    lv_label_set_text(status_label, "Connecting...");
                    lv_obj_set_style_text_color(status_label,
                                                 lv_color_hex(0xF39C12), 0);
                    lv_label_set_text(ip_label, "---");
                    lv_bar_set_value(rssi_bar, -100, LV_ANIM_OFF);
                    break;

                case WiFiService::WIFI_ERROR:
                    lv_label_set_text(status_label, "Error");
                    lv_obj_set_style_text_color(status_label,
                                                 lv_color_hex(0xE74C3C), 0);
                    lv_label_set_text(ip_label, "Connection failed");
                    break;

                default:
                    lv_label_set_text(status_label, "Disconnected");
                    lv_obj_set_style_text_color(status_label,
                                                 lv_color_hex(0x95A5A6), 0);
                    lv_label_set_text(ip_label, "---");
                    lv_bar_set_value(rssi_bar, -100, LV_ANIM_OFF);
                    break;
            }
        });
    }

    void onEvent(const char* event, void* data) override {
        Serial.printf("[WiFiWidget] Event: %s\n", event);
        update();  // Aggiorna UI quando arriva evento
    }
};
```

---

### 3.3 System Info Widget

```cpp
// widgets/system_info_widget.h
#pragma once
#include "dashboard_widget.h"
#include <esp_heap_caps.h>

class SystemInfoWidget : public DashboardWidget {
private:
    lv_obj_t* cpu_label = nullptr;
    lv_obj_t* mem_label = nullptr;
    lv_obj_t* uptime_label = nullptr;
    lv_obj_t* mem_bar = nullptr;

    uint32_t start_time_ms = 0;

public:
    SystemInfoWidget() {
        span_cols = 1;
        span_rows = 1;
        start_time_ms = millis();
    }

    void create(lv_obj_t* parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, LV_PCT(100), 120);
        lv_obj_set_style_radius(container, 10, 0);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x34495E), 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(container, 12, 0);
        lv_obj_set_style_pad_gap(container, 8, 0);

        // Title
        lv_obj_t* title = lv_label_create(container);
        lv_label_set_text(title, LV_SYMBOL_SETTINGS " System");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

        // CPU info
        cpu_label = lv_label_create(container);
        lv_obj_set_style_text_color(cpu_label, lv_color_hex(0xECF0F1), 0);
        lv_obj_set_style_text_font(cpu_label, &lv_font_montserrat_12, 0);

        // Memory bar
        mem_bar = lv_bar_create(container);
        lv_obj_set_size(mem_bar, LV_PCT(100), 12);
        lv_obj_set_style_bg_color(mem_bar, lv_color_hex(0x1C2833), 0);

        // Memory label
        mem_label = lv_label_create(container);
        lv_obj_set_style_text_color(mem_label, lv_color_hex(0xECF0F1), 0);
        lv_obj_set_style_text_font(mem_label, &lv_font_montserrat_10, 0);

        // Uptime
        uptime_label = lv_label_create(container);
        lv_obj_set_style_text_color(uptime_label, lv_color_hex(0xBDC3C7), 0);
        lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_10, 0);

        update();
    }

    void update() override {
        safeUpdateUI([this]() {
            // CPU info
            lv_label_set_text_fmt(cpu_label, "ESP32-S3 @ %d MHz",
                                  ESP.getCpuFreqMHz());

            // Memory info
            size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
            size_t used_heap = total_heap - free_heap;
            uint8_t mem_pct = (used_heap * 100) / total_heap;

            lv_bar_set_value(mem_bar, mem_pct, LV_ANIM_ON);
            lv_label_set_text_fmt(mem_label, "RAM: %d KB / %d KB",
                                  used_heap / 1024, total_heap / 1024);

            // Colore barra memoria
            if (mem_pct < 70) {
                lv_obj_set_style_bg_color(mem_bar, lv_color_hex(0x2ECC71),
                                          LV_PART_INDICATOR);
            } else if (mem_pct < 90) {
                lv_obj_set_style_bg_color(mem_bar, lv_color_hex(0xF39C12),
                                          LV_PART_INDICATOR);
            } else {
                lv_obj_set_style_bg_color(mem_bar, lv_color_hex(0xE74C3C),
                                          LV_PART_INDICATOR);
            }

            // Uptime
            uint32_t uptime_sec = (millis() - start_time_ms) / 1000;
            uint32_t hours = uptime_sec / 3600;
            uint32_t minutes = (uptime_sec % 3600) / 60;
            uint32_t seconds = uptime_sec % 60;

            lv_label_set_text_fmt(uptime_label, "Uptime: %02d:%02d:%02d",
                                  hours, minutes, seconds);
        });
    }
};
```

---

### 3.4 Clock Widget

```cpp
// widgets/clock_widget.h
#pragma once
#include "dashboard_widget.h"
#include <time.h>

class ClockWidget : public DashboardWidget {
private:
    lv_obj_t* time_label = nullptr;
    lv_obj_t* date_label = nullptr;
    bool time_synced = false;

public:
    ClockWidget() {
        span_cols = 2;
        span_rows = 1;
    }

    void create(lv_obj_t* parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, LV_PCT(100), 100);
        lv_obj_set_style_radius(container, 10, 0);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x8E44AD), 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Time
        time_label = lv_label_create(container);
        lv_label_set_text(time_label, "--:--");
        lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(time_label, &lv_font_montserrat_32, 0);

        // Date
        date_label = lv_label_create(container);
        lv_label_set_text(date_label, "--- -- ----");
        lv_obj_set_style_text_color(date_label, lv_color_hex(0xECF0F1), 0);
        lv_obj_set_style_text_font(date_label, &lv_font_montserrat_14, 0);

        update();
    }

    void update() override {
        safeUpdateUI([this]() {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                time_synced = true;

                // Format time
                char time_str[16];
                strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
                lv_label_set_text(time_label, time_str);

                // Format date
                char date_str[32];
                strftime(date_str, sizeof(date_str), "%a %d %b %Y", &timeinfo);
                lv_label_set_text(date_label, date_str);
            } else {
                lv_label_set_text(time_label, "--:--:--");
                lv_label_set_text(date_label, "Time not synced");
            }
        });
    }

    void syncWithNTP(const char* ntp_server = "pool.ntp.org") {
        configTime(0, 0, ntp_server);
        Serial.println("[Clock] Syncing with NTP...");
    }
};
```

---

### 3.5 Custom Dashboard Screen

```cpp
// screens/custom_dashboard_screen.h
#pragma once
#include "screen.h"
#include "dashboard_widget.h"
#include "widgets/wifi_status_widget.h"
#include "widgets/system_info_widget.h"
#include "widgets/clock_widget.h"
#include <vector>

class CustomDashboard : public Screen {
private:
    lv_obj_t* grid_container = nullptr;
    lv_obj_t* header = nullptr;
    std::vector<DashboardWidget*> widgets;
    lv_timer_t* update_timer = nullptr;

    static void updateWidgetsCallback(lv_timer_t* timer) {
        CustomDashboard* dashboard = (CustomDashboard*)timer->user_data;
        dashboard->updateAllWidgets();
    }

public:
    CustomDashboard() : Screen("Custom Dashboard") {}

    ~CustomDashboard() {
        if (update_timer) {
            lv_timer_del(update_timer);
        }
        for (auto widget : widgets) {
            delete widget;
        }
    }

    void addWidget(DashboardWidget* widget) {
        widgets.push_back(widget);
    }

    void onCreate() override {
        screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x1C1C1C), 0);

        // Header
        createHeader();

        // Grid per widget
        createWidgetGrid();

        // Aggiungi widget
        for (auto widget : widgets) {
            widget->create(grid_container);
        }

        // Timer per aggiornamenti periodici (ogni 2 secondi)
        update_timer = lv_timer_create(updateWidgetsCallback, 2000, this);
    }

    void onResume() override {
        updateAllWidgets();
        if (update_timer) {
            lv_timer_resume(update_timer);
        }
    }

    void onPause() override {
        if (update_timer) {
            lv_timer_pause(update_timer);
        }
    }

private:
    void createHeader() {
        header = lv_obj_create(screen);
        lv_obj_set_size(header, LV_PCT(100), 60);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_radius(header, 0, 0);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, LV_SYMBOL_HOME " Dashboard");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
        lv_obj_center(title);
    }

    void createWidgetGrid() {
        grid_container = lv_obj_create(screen);
        lv_obj_set_size(grid_container, LV_PCT(95), LV_PCT(85));
        lv_obj_align(grid_container, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_bg_opa(grid_container, 0, 0);
        lv_obj_set_style_border_width(grid_container, 0, 0);
        lv_obj_set_style_pad_all(grid_container, 5, 0);
        lv_obj_set_style_pad_gap(grid_container, 10, 0);

        // Grid layout 3 colonne
        static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                        LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT,
                                        LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

        lv_obj_set_layout(grid_container, LV_LAYOUT_GRID);
        lv_obj_set_grid_dsc_array(grid_container, col_dsc, row_dsc);
    }

    void updateAllWidgets() {
        for (auto widget : widgets) {
            widget->update();
        }
    }
};
```

---

## 4. Inizializzazione Completa del Sistema

```cpp
// main.cpp
#include <Arduino.h>
#include <lvgl.h>
#include "service_manager.h"
#include "wifi_service.h"
#include "ble_service.h"
#include "screen_manager.h"
#include "app_manager.h"
#include "settings_manager.h"
#include "screens/custom_dashboard_screen.h"
#include "widgets/wifi_status_widget.h"
#include "widgets/system_info_widget.h"
#include "widgets/clock_widget.h"

// Global LVGL mutex
SemaphoreHandle_t lvgl_mutex = NULL;

// LVGL display e driver
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

// Touch input
static lv_indev_drv_t indev_drv;

// Services
WiFiService* wifi_service = nullptr;
BLEService* ble_service = nullptr;

// LVGL Task
static void lvgl_task(void* pvParameter) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(lvgl_mutex);
        }
    }
}

// LVGL Tick
static void lv_tick_task(void *arg) {
    lv_tick_inc(1);
}

void setup_lvgl() {
    Serial.println("[Setup] Initializing LVGL...");

    lv_init();

    // Alloca buffer in PSRAM (ESP32-S3 ha PSRAM!)
    size_t buf_size = LV_HOR_RES_MAX * LV_VER_RES_MAX / 10;
    buf1 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t),
                                         MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t),
                                         MALLOC_CAP_SPIRAM);

    if (!buf1 || !buf2) {
        Serial.println("[ERROR] Failed to allocate LVGL buffers!");
        return;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

    // Display driver (TFT_eSPI)
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = tft_flush_cb;  // Implementato con TFT_eSPI
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    lv_disp_drv_register(&disp_drv);

    // Touch driver (FT6336U)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;  // Implementato con FT6336U
    lv_indev_drv_register(&indev_drv);

    // Crea mutex globale
    lvgl_mutex = xSemaphoreCreateMutex();

    // Timer tick 1ms
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));

    // LVGL task su Core 1
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 3, NULL, 1);

    Serial.println("[Setup] LVGL initialized!");
}

void setup_services() {
    Serial.println("[Setup] Initializing services...");

    auto service_mgr = ServiceManager::getInstance();

    // WiFi Service
    wifi_service = new WiFiService();
    wifi_service->configure("YOUR_SSID", "YOUR_PASSWORD");
    service_mgr->registerService("wifi", wifi_service);

    // BLE Service
    ble_service = new BLEService("ESP32_OS");
    service_mgr->registerService("ble", ble_service);

    // Avvia tutti i servizi
    service_mgr->startAll();

    // Connetti WiFi
    wifi_service->connect();

    // Avvia BLE advertising
    ble_service->startAdvertising();

    service_mgr->printStatus();
}

void setup_dashboard() {
    Serial.println("[Setup] Creating dashboard...");

    // Crea dashboard con widget
    auto dashboard = new CustomDashboard();

    // Aggiungi widget
    dashboard->addWidget(new ClockWidget());
    dashboard->addWidget(new WiFiStatusWidget());
    dashboard->addWidget(new SystemInfoWidget());

    // Carica dashboard
    ScreenManager::getInstance()->pushScreen(dashboard);

    // Sync NTP (se WiFi connesso)
    EventRouter::getInstance()->subscribe("wifi.connected",
        [](const char* event, void* data) {
            // Aspetta un po' per stabilità WiFi
            vTaskDelay(pdMS_TO_TICKS(2000));

            // Sync NTP
            configTime(0, 0, "pool.ntp.org");
            Serial.println("[NTP] Time sync requested");
        });
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== ESP32 OS-like System Starting ===\n");

    // 1. Init hardware (TFT, Touch, SD, etc.)
    // ... (vedi REPORT_ARCHITETTURA_OS_ESP32.md)

    // 2. Init LVGL
    setup_lvgl();

    // 3. Init core managers
    SettingsManager::getInstance()->begin();

    // 4. Init services
    setup_services();

    // 5. Setup dashboard
    setup_dashboard();

    Serial.println("\n=== System Ready! ===\n");
}

void loop() {
    // FreeRTOS gestisce tutto, loop vuoto
    vTaskDelay(portMAX_DELAY);
}
```

---

## 5. Architettura Finale - Layer Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                            │
│                                                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ Dashboard    │  │ Settings     │  │ Custom Apps  │          │
│  │ Screen       │  │ Screen       │  │              │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                  │                  │                   │
│         └──────────────────┴──────────────────┘                   │
│                            │                                       │
│                  ┌─────────▼──────────┐                           │
│                  │  Widget System      │                           │
│                  │  - WiFi Widget      │                           │
│                  │  - System Widget    │                           │
│                  │  - Clock Widget     │                           │
│                  └─────────┬──────────┘                           │
└────────────────────────────┼──────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                   SYSTEM SERVICES LAYER (NEW!)                     │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │             ServiceManager (Registry)                         │ │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐             │ │
│  │  │ WiFiService│  │ BLEService │  │ Future...  │             │ │
│  │  │ Core 0     │  │ Core 0     │  │            │             │ │
│  │  │ Priority 5 │  │ Priority 5 │  │            │             │ │
│  │  └────────────┘  └────────────┘  └────────────┘             │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                            │                                        │
│                  ┌─────────▼──────────┐                            │
│                  │  EventRouter        │                            │
│                  │  (Pub/Sub)          │                            │
│                  └─────────┬──────────┘                            │
└────────────────────────────┼──────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                   CORE MANAGERS LAYER                              │
│                                                                     │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                  │
│  │ Screen     │  │ App        │  │ Settings   │                  │
│  │ Manager    │  │ Manager    │  │ Manager    │                  │
│  └────────────┘  └────────────┘  └────────────┘                  │
└────────────────────────────────────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                   GUI FRAMEWORK LAYER                              │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │  LVGL v8.4.0                                                  │ │
│  │  - Task on Core 1                                             │ │
│  │  - Priority 3                                                 │ │
│  │  - Thread-safe with Mutex                                     │ │
│  │  - lv_task_handler() ogni 10ms                                │ │
│  │  - lv_tick_inc() ogni 1ms (timer)                             │ │
│  └──────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                   HARDWARE ABSTRACTION LAYER                       │
│                                                                     │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                  │
│  │ TFT_eSPI   │  │ FT6336U    │  │ WiFi/BLE   │                  │
│  │ Display    │  │ Touch      │  │ Stack      │                  │
│  └────────────┘  └────────────┘  └────────────┘                  │
└────────────────────────────────────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                        HARDWARE                                    │
│  ESP32-S3 Dual Core (PRO CPU + APP CPU) + PSRAM                   │
└────────────────────────────────────────────────────────────────────┘
```

---

## 6. FreeRTOS Task Map

```
Core 0 (PRO CPU)                  Core 1 (APP CPU)
┌──────────────────┐              ┌──────────────────┐
│                  │              │                  │
│ WiFi Stack       │              │ LVGL Task        │
│ (ESP32 default)  │              │ Priority: 3      │
│                  │              │ Stack: 8KB       │
├──────────────────┤              │ 10ms loop        │
│                  │              └──────────────────┘
│ WiFiService      │
│ Priority: 5      │              ┌──────────────────┐
│ Stack: 4KB       │              │ App Tasks        │
│ 500ms loop       │              │ (future)         │
├──────────────────┤              │                  │
│                  │              └──────────────────┘
│ BLEService       │
│ Priority: 5      │
│ Stack: 8KB       │
│ Event-driven     │
├──────────────────┤
│                  │
│ Other Services   │
│ (future)         │
│                  │
└──────────────────┘

Timer Interrupts (both cores):
┌──────────────────────────────┐
│ LVGL Tick Timer              │
│ 1ms periodic                 │
│ Calls: lv_tick_inc(1)        │
└──────────────────────────────┘
```

---

## 7. Sincronizzazione Thread-Safe - Regole d'Oro

### ✅ DA FARE SEMPRE

```cpp
// 1. Usa SEMPRE il mutex prima di chiamare funzioni LVGL
void wifi_task_example() {
    // Ottieni dati dal WiFi (OK senza mutex)
    int rssi = WiFi.RSSI();

    // LOCK prima di aggiornare UI
    if (pdTRUE == xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100))) {
        lv_label_set_text_fmt(label, "RSSI: %d", rssi);
        xSemaphoreGive(lvgl_mutex);
    }
}

// 2. Minimizza il tempo con mutex acquisito
void good_practice() {
    // Prepara dati FUORI dal mutex
    String text = prepareComplexString();

    // LOCK solo per UI update
    if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
        lv_label_set_text(label, text.c_str());
        xSemaphoreGive(lvgl_mutex);  // Rilascia subito
    }
}

// 3. Usa EventRouter per comunicazione asincrona
void async_pattern() {
    // Dal WiFi task (Core 0)
    EventRouter::getInstance()->publish("wifi.rssi", (void*)&rssi);

    // Nel Widget (LVGL Core 1) - già thread-safe con widget->update()
    void WiFiWidget::onEvent(const char* event, void* data) {
        // Widget::update() usa già safeUpdateUI() con mutex
        update();
    }
}
```

### ❌ DA NON FARE MAI

```cpp
// SBAGLIATO: Chiamata LVGL senza mutex da altro task
void wifi_task_BAD() {
    lv_label_set_text(label, "text");  // CRASH PROBABILE!
}

// SBAGLIATO: Tenere mutex troppo a lungo
void bad_practice() {
    if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
        doComplexCalculation();  // NO! Altri task bloccati
        vTaskDelay(1000);        // NO! LVGL congelato
        lv_label_set_text(label, "text");
        xSemaphoreGive(lvgl_mutex);
    }
}

// SBAGLIATO: Nessun timeout su mutex
void deadlock_risk() {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);  // Può bloccare forever
    // Se c'è eccezione qui, mutex mai rilasciato!
    lv_label_set_text(label, "text");
    xSemaphoreGive(lvgl_mutex);
}
```

---

## 8. Problemi Comuni e Soluzioni

### Problema 1: UI Freeze

**Sintomo:** GUI si blocca dopo un po'

**Cause:**
- Mutex non rilasciato
- Stack overflow LVGL task
- Deadlock tra task

**Debug:**
```cpp
// Aggiungi logging mutex
if (pdTRUE == xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100))) {
    Serial.println("[MUTEX] Acquired by " + String(pcTaskGetName(NULL)));
    lv_task_handler();
    xSemaphoreGive(lvgl_mutex);
    Serial.println("[MUTEX] Released");
} else {
    Serial.println("[MUTEX] Timeout! Possible deadlock");
}

// Monitora stack
UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
Serial.printf("[Stack] Free: %d bytes\n", stackHighWaterMark);
```

---

### Problema 2: WiFi/BLE Lento

**Sintomo:** Connessioni WiFi/BLE lente o instabili

**Causa:** Task LVGL monopolizza CPU

**Soluzione:**
```cpp
// Riduci priorità LVGL, aumenta WiFi
xTaskCreatePinnedToCore(wifi_task, "wifi", 4096, NULL, 6, NULL, 0);  // Priority 6
xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 2, NULL, 1);  // Priority 2

// Oppure aggiungi yield in LVGL task
static void lvgl_task(void* pvParameter) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(lvgl_mutex);
        }

        taskYIELD();  // Permetti altri task di girare
    }
}
```

---

### Problema 3: Memory Leak

**Sintomo:** Heap si riduce progressivamente

**Debug:**
```cpp
void printMemoryStats() {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Min free heap: %d bytes\n", ESP.getMinFreeHeap());
    Serial.printf("Largest free block: %d bytes\n",
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // LVGL memory monitor
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("LVGL: %d%% used, %d KB total\n",
                  mon.used_pct, mon.total_size / 1024);
}

// Chiamala periodicamente
EventRouter::getInstance()->subscribe("system.tick",
    [](const char* e, void* d) {
        static int counter = 0;
        if (++counter % 30 == 0) {  // Ogni 30 secondi
            printMemoryStats();
        }
    });
```

---

## 9. Checklist Estensione

Prima di aggiungere un nuovo servizio o widget:

- [ ] Il servizio è su **Core 0** se usa WiFi/BLE?
- [ ] LVGL task è sempre su **Core 1**?
- [ ] **Priorità corrette** (WiFi/BLE > App > LVGL)?
- [ ] **Stack size adeguato** (WiFi/BLE 8KB, LVGL 8KB)?
- [ ] Tutti gli update UI usano il **mutex LVGL**?
- [ ] Eventi usano **EventRouter** invece di chiamate dirette?
- [ ] Cleanup corretto in `onStop()` e distruttori?
- [ ] Memory usage monitorato?
- [ ] Timeout su `xSemaphoreTake()` (no `portMAX_DELAY` dove rischioso)?

---

## 10. Conclusioni

### Estensioni Implementate

✅ **Service Layer completo**
- Pattern base `SystemService`
- `WiFiService` con gestione stato e auto-reconnect
- `BLEService` con advertising
- `ServiceManager` per registro centralizzato

✅ **Widget System customizzabile**
- `DashboardWidget` base class
- Widget WiFi, System Info, Clock pronti all'uso
- Dashboard con layout grid flessibile

✅ **Thread-safety verificata**
- Mutex LVGL globale
- Core pinning (WiFi Core 0, LVGL Core 1)
- Priorità task ottimizzate
- Pattern EventRouter per comunicazione asincrona

✅ **Best practices dalla community**
- Ispirato da Tactility OS (service lifecycle)
- Pattern ufficiale LVGL + FreeRTOS
- Soluzioni problemi comuni (freeze, memory, WiFi conflicts)

### Architettura Scalabile

L'architettura ora supporta:
- Aggiunta di nuovi servizi (es. MQTT, HTTP Server, NTP)
- Widget custom nella dashboard
- Background tasks thread-safe
- Event-driven communication
- Memory management ottimizzato con PSRAM

### Prossimi Passi

1. Implementare app Settings per configurare WiFi/BLE
2. Aggiungere widget battery monitor (se hardware disponibile)
3. Implementare NTP service per clock sync
4. Aggiungere MQTT service per IoT
5. File manager per SD card

---

**Documento:** ESTENSIONI_ARCHITETTURA_SERVIZI.md
**Versione:** 1.0
**Data:** 2025-11-14
**Integra:** [REPORT_ARCHITETTURA_OS_ESP32.md](REPORT_ARCHITETTURA_OS_ESP32.md)
**Licenze:** Vedi [ANALISI_LICENZE.md](ANALISI_LICENZE.md)

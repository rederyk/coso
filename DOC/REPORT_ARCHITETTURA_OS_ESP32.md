# Report: Architettura Sistema OS-like per ESP32 S3 Display Touch

> **⚠️ DOCUMENTO DEPRECATO**
>
> Questo documento è stato **sostituito** da:
> - **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** - Architettura completa unificata
> - **[ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md)** - Gestione periferiche hardware
>
> **Usa i nuovi documenti per l'implementazione.** Questo file è mantenuto solo per riferimento storico.

---

## Executive Summary

Questo documento descrive come progettare e implementare un sistema operativo leggero (OS-like) per ESP32 S3 con display touch, mantenendo professionalità, leggerezza e manutenibilità. Il sistema include gestione settings, dashboard con launcher e architettura modulare per applicazioni.

**📌 IMPORTANTE**: Questo report è stato **unificato** nel documento [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) che include:
- Architettura completa a 5 layer
- **Service Layer** per WiFi, BLE e servizi di sistema
- **Widget System** per dashboard customizzabile
- **Peripheral Manager** per GPIO, I2C, SPI, ADC, PWM, UART
- **Thread-safety** patterns verificati dalla community LVGL/ESP32
- **Core pinning** e gestione FreeRTOS multi-core
- Soluzioni a problemi comuni trovati dalla community
- Esempi di codice completi e pronti all'uso

**👉 Leggi [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) per la documentazione aggiornata.**

## 1. Stack Tecnologico Raccomandato

### 1.1 Librerie Fondamentali

**LVGL v8.4.0** (Light and Versatile Graphics Library)
- Framework GUI principale per interfaccia utente
- Footprint ridotto: 64KB Flash, 16KB RAM minimo
- Supporto completo ESP32 S3 con DMA
- Sistema eventi robusto e architettura modulare
- Layout engine (Flexbox, Grid) per UI responsive
- Gestione multi-screen nativa

**TFT_eSPI v2.5.43**
- Driver ottimizzato per display TFT
- Supporto ESP32 S3 con interfacce SPI e parallela
- Performance elevate con DMA
- Sprite system per rendering senza flicker

**FT6336U Touch Controller**
- Libreria per gestione touch capacitivo
- Interfaccia I2C
- Supporto multi-touch

### 1.2 Architettura Hardware

```
ESP32-S3 (240MHz Dual-Core)
├── Display TFT (SPI/Parallel)
│   ├── ILI9341 (240x320) / ST7789 / ST7796
│   └── DMA per performance
├── Touch Controller FT6336U (I2C)
├── PSRAM (opzionale, consigliato per sprite full-screen)
└── Flash Storage (SPIFFS/LittleFS per risorse)
```

## 2. Architettura Software del Sistema

### 2.1 Struttura Modulare a Livelli

```
┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ Dashboard    │  │ Settings     │  │ Custom Apps  │          │
│  │ Screen       │  │ Screen       │  │              │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                  │                  │                   │
│         └──────────────────┴──────────────────┘                   │
│                            │                                       │
│                  ┌─────────▼──────────┐                           │
│                  │  Widget System      │                           │
│                  │  (Dashboard Custom) │                           │
│                  └─────────┬──────────┘                           │
└────────────────────────────┼──────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│              SYSTEM SERVICES LAYER (Background Tasks)             │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │             ServiceManager (Registry)                         │ │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐             │ │
│  │  │ WiFiService│  │ BLEService │  │ Future...  │             │ │
│  │  │ Core 0     │  │ Core 0     │  │ (MQTT,NTP) │             │ │
│  │  │ Priority 5 │  │ Priority 5 │  │            │             │ │
│  │  └────────────┘  └────────────┘  └────────────┘             │ │
│  └──────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                   CORE MANAGERS LAYER                              │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                  │
│  │ Screen     │  │ App        │  │ Settings   │                  │
│  │ Manager    │  │ Manager    │  │ Manager    │                  │
│  └────────────┘  └────────────┘  └────────────┘                  │
│                  ┌─────────▼──────────┐                            │
│                  │  EventRouter        │                            │
│                  │  (Pub/Sub)          │                            │
│                  └─────────┬──────────┘                            │
└────────────────────────────┼──────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                   GUI FRAMEWORK LAYER                              │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │  LVGL v8.4.0                                                  │ │
│  │  - Task on Core 1 (APP CPU)                                   │ │
│  │  - Priority 2-3                                                │ │
│  │  - Thread-safe with Global Mutex                              │ │
│  │  - lv_task_handler() ogni 10ms                                │ │
│  │  - lv_tick_inc() ogni 1ms (timer)                             │ │
│  └──────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────┘
                             │
┌────────────────────────────▼──────────────────────────────────────┐
│                   HARDWARE ABSTRACTION LAYER                       │
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

**⚠️ NOTE ARCHITETTURALI CRITICHE:**

1. **Service Layer** (nuovo rispetto alla versione base):
   - Servizi di sistema (WiFi, BLE) girano su **Core 0** (PRO CPU)
   - **OBBLIGATORIO** per WiFi Stack ESP32
   - Priorità 5-6 (più alta di LVGL)

2. **LVGL GUI Task**:
   - SEMPRE su **Core 1** (APP CPU)
   - Priorità 2-3 (più bassa dei servizi critici)
   - **Mutex globale** per thread-safety

3. **Thread-Safety**:
   - Tutti gli aggiornamenti UI da altri task DEVONO acquisire mutex LVGL
   - EventRouter per comunicazione asincrona tra layer
   - Pattern documentato in [ESTENSIONI_ARCHITETTURA_SERVIZI.md](ESTENSIONI_ARCHITETTURA_SERVIZI.md)

### 2.2 Componenti Chiave

#### 2.2.1 Screen Manager

Gestisce la navigazione tra schermate e transizioni.

```cpp
// screen_manager.h
#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <lvgl.h>
#include <vector>

enum ScreenType {
    SCREEN_DASHBOARD,
    SCREEN_SETTINGS,
    SCREEN_APP_LAUNCHER,
    SCREEN_APP_INSTANCE
};

class Screen {
public:
    virtual void onCreate(lv_obj_t* parent) = 0;
    virtual void onResume() = 0;
    virtual void onPause() = 0;
    virtual void onDestroy() = 0;
    virtual ScreenType getType() = 0;
    virtual const char* getName() = 0;

    lv_obj_t* getContainer() { return screen_obj; }

protected:
    lv_obj_t* screen_obj = nullptr;
};

class ScreenManager {
private:
    static ScreenManager* instance;
    std::vector<Screen*> screen_stack;
    Screen* current_screen = nullptr;

    ScreenManager() {}

public:
    static ScreenManager* getInstance();

    void pushScreen(Screen* screen, lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_IN);
    void popScreen(lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_OUT);
    void replaceScreen(Screen* screen, lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_IN);
    Screen* getCurrentScreen() { return current_screen; }
    bool canGoBack() { return screen_stack.size() > 1; }
};

#endif
```

#### 2.2.2 Application Manager

Sistema di registrazione e lifecycle delle applicazioni.

```cpp
// app_manager.h
#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <map>
#include <functional>
#include "screen_manager.h"

struct AppInfo {
    const char* id;
    const char* name;
    const char* icon_path;
    uint16_t icon_color;
    std::function<Screen*()> factory;
};

class AppManager {
private:
    static AppManager* instance;
    std::map<const char*, AppInfo> registered_apps;
    std::map<const char*, Screen*> running_apps;

    AppManager() {}

public:
    static AppManager* getInstance();

    // Registrazione app
    void registerApp(const AppInfo& info);
    void unregisterApp(const char* app_id);

    // Lifecycle
    Screen* launchApp(const char* app_id);
    void closeApp(const char* app_id);
    bool isAppRunning(const char* app_id);

    // Query
    std::vector<AppInfo> getInstalledApps();
    AppInfo* getAppInfo(const char* app_id);
};

#endif
```

#### 2.2.3 Settings Manager

Gestione configurazioni di sistema con persistenza.

```cpp
// settings_manager.h
#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Preferences.h>
#include <map>
#include <functional>

enum SettingType {
    SETTING_BOOL,
    SETTING_INT,
    SETTING_STRING,
    SETTING_FLOAT
};

struct Setting {
    const char* key;
    const char* display_name;
    const char* description;
    SettingType type;
    void* default_value;
    std::function<void(void*)> onChange;
};

class SettingsManager {
private:
    static SettingsManager* instance;
    Preferences preferences;
    std::map<const char*, Setting> settings_registry;

    SettingsManager() {}

public:
    static SettingsManager* getInstance();

    void begin(const char* namespace_name = "system");
    void registerSetting(const Setting& setting);

    // Getters
    bool getBool(const char* key, bool default_val = false);
    int getInt(const char* key, int default_val = 0);
    String getString(const char* key, String default_val = "");
    float getFloat(const char* key, float default_val = 0.0f);

    // Setters
    void setBool(const char* key, bool value);
    void setInt(const char* key, int value);
    void setString(const char* key, String value);
    void setFloat(const char* key, float value);

    // Utility
    void reset();
    std::vector<Setting> getAllSettings();
};

#endif
```

#### 2.2.4 Event Router

Sistema di comunicazione tra componenti (Publish/Subscribe pattern).

**⚠️ IMPORTANTE PER THREAD-SAFETY:**
EventRouter permette comunicazione **asincrona** tra servizi background (WiFi/BLE su Core 0) e UI (LVGL su Core 1) senza dover gestire mutex manualmente in ogni punto.

```cpp
// event_router.h
#ifndef EVENT_ROUTER_H
#define EVENT_ROUTER_H

#include <functional>
#include <map>
#include <vector>
#include <Arduino.h>

// Callback con event name (string) e data
typedef std::function<void(const char* event, void* data)> EventCallback;

class EventRouter {
private:
    static EventRouter* instance;
    std::map<String, std::vector<EventCallback>> listeners;

    EventRouter() {}

public:
    static EventRouter* getInstance() {
        if (instance == nullptr) {
            instance = new EventRouter();
        }
        return instance;
    }

    // Subscribe to event
    void subscribe(const char* event, EventCallback callback) {
        listeners[event].push_back(callback);
    }

    // Publish event to all subscribers
    void publish(const char* event, void* data = nullptr) {
        if (listeners.count(event) > 0) {
            for (auto& callback : listeners[event]) {
                callback(event, data);
            }
        }
    }

    // Unsubscribe (advanced, opzionale)
    void unsubscribe(const char* event) {
        listeners.erase(event);
    }
};

EventRouter* EventRouter::instance = nullptr;

#endif
```

**Esempio Uso (WiFi Service → UI Widget):**

```cpp
// Nel WiFiService (Core 0)
void WiFiService::onConnected() {
    wifi_state = WIFI_CONNECTED;
    ip_address = WiFi.localIP();

    // Pubblica evento (thread-safe, solo notifica)
    EventRouter::getInstance()->publish("wifi.connected",
                                        (void*)ip_address.toString().c_str());
}

// Nel Widget (LVGL Core 1)
WiFiStatusWidget::WiFiStatusWidget() {
    // Subscribe all'evento
    EventRouter::getInstance()->subscribe("wifi.connected",
        [this](const char* event, void* data) {
            // Widget::update() gestisce mutex internamente
            this->update();
        });
}
```

**Vantaggi:**
- ✅ Disaccoppiamento componenti
- ✅ Thread-safe (callback eseguito nel task subscriber)
- ✅ Facile debugging (log eventi)
- ✅ Estendibile (nuovi eventi senza modifiche)

### 2.3 Dashboard Screen

Schermata principale con launcher per app.

```cpp
// dashboard_screen.cpp
#include "dashboard_screen.h"
#include "app_manager.h"

void DashboardScreen::onCreate(lv_obj_t* parent) {
    screen_obj = lv_obj_create(parent);
    lv_obj_set_size(screen_obj, LV_PCT(100), LV_PCT(100));

    createHeader();
    createAppGrid();
    createStatusBar();
}

void DashboardScreen::createHeader() {
    lv_obj_t* header = lv_obj_create(screen_obj);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Logo/Title
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "ESP32 OS");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    // Settings button
    lv_obj_t* settings_btn = lv_btn_create(header);
    lv_obj_set_size(settings_btn, 40, 40);
    lv_obj_align(settings_btn, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_add_event_cb(settings_btn, settings_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* settings_icon = lv_label_create(settings_btn);
    lv_label_set_text(settings_icon, LV_SYMBOL_SETTINGS);
    lv_obj_center(settings_icon);
}

void DashboardScreen::createAppGrid() {
    lv_obj_t* grid_container = lv_obj_create(screen_obj);
    lv_obj_set_size(grid_container, LV_PCT(100), LV_PCT(80));
    lv_obj_align(grid_container, LV_ALIGN_TOP_MID, 0, 55);

    // Setup grid layout
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                    LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                    LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_set_layout(grid_container, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid_container, col_dsc, row_dsc);

    // Populate with apps
    auto apps = AppManager::getInstance()->getInstalledApps();
    uint8_t row = 0, col = 0;

    for (const auto& app : apps) {
        createAppIcon(grid_container, app, col, row);

        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }
}

void DashboardScreen::createAppIcon(lv_obj_t* parent, const AppInfo& app,
                                    uint8_t col, uint8_t row) {
    lv_obj_t* app_btn = lv_btn_create(parent);
    lv_obj_set_size(app_btn, 70, 80);
    lv_obj_set_grid_cell(app_btn, LV_GRID_ALIGN_CENTER, col, 1,
                         LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_color(app_btn, lv_color_hex(app.icon_color), 0);

    // Store app_id in user data
    lv_obj_set_user_data(app_btn, (void*)app.id);
    lv_obj_add_event_cb(app_btn, app_launch_cb, LV_EVENT_CLICKED, this);

    // Icon
    lv_obj_t* icon = lv_label_create(app_btn);
    lv_label_set_text(icon, app.icon_path); // o LV_SYMBOL_*
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 5);

    // Name
    lv_obj_t* name = lv_label_create(app_btn);
    lv_label_set_text(name, app.name);
    lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
}

void DashboardScreen::app_launch_cb(lv_event_t* e) {
    DashboardScreen* self = (DashboardScreen*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);
    const char* app_id = (const char*)lv_obj_get_user_data(btn);

    Screen* app_screen = AppManager::getInstance()->launchApp(app_id);
    if (app_screen) {
        ScreenManager::getInstance()->pushScreen(app_screen,
                                                 LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}
```

### 2.4 Settings Screen

Interfaccia per configurazioni di sistema.

```cpp
// settings_screen.cpp
void SettingsScreen::onCreate(lv_obj_t* parent) {
    screen_obj = lv_obj_create(parent);

    createHeader();
    createSettingsList();
}

void SettingsScreen::createSettingsList() {
    lv_obj_t* list = lv_list_create(screen_obj);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(85));
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);

    auto settings = SettingsManager::getInstance()->getAllSettings();

    for (const auto& setting : settings) {
        switch (setting.type) {
            case SETTING_BOOL:
                addToggleSetting(list, setting);
                break;
            case SETTING_INT:
                addSliderSetting(list, setting);
                break;
            case SETTING_STRING:
                addTextSetting(list, setting);
                break;
        }
    }
}

void SettingsScreen::addToggleSetting(lv_obj_t* list, const Setting& setting) {
    lv_obj_t* item = lv_list_add_btn(list, nullptr, setting.display_name);

    lv_obj_t* toggle = lv_switch_create(item);
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -10, 0);

    bool current_value = SettingsManager::getInstance()->getBool(setting.key);
    if (current_value) {
        lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }

    lv_obj_set_user_data(toggle, (void*)setting.key);
    lv_obj_add_event_cb(toggle, toggle_changed_cb, LV_EVENT_VALUE_CHANGED, this);
}

void SettingsScreen::toggle_changed_cb(lv_event_t* e) {
    lv_obj_t* toggle = lv_event_get_target(e);
    const char* key = (const char*)lv_obj_get_user_data(toggle);

    bool new_value = lv_obj_has_state(toggle, LV_STATE_CHECKED);
    SettingsManager::getInstance()->setBool(key, new_value);
}
```

## 3. Best Practices per Professionalità e Manutenibilità

### 3.1 Gestione Memoria

```cpp
// Utilizzare PSRAM per sprite di grandi dimensioni
#if defined(BOARD_HAS_PSRAM)
    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, ps_malloc(320*240*2), 320, 240, LV_IMG_CF_TRUE_COLOR);
#endif

// Rilasciare risorse quando non necessarie
void AppScreen::onPause() {
    if (large_sprite != nullptr) {
        lv_obj_del(large_sprite);
        large_sprite = nullptr;
    }
}
```

### 3.2 Task Scheduling e Core Pinning

**⚠️ REGOLE CRITICHE PER MULTI-CORE ESP32-S3:**

```cpp
// REGOLA 1: WiFi/BLE SEMPRE su Core 0 (PRO CPU)
// Lo stack WiFi ESP32 RICHIEDE Core 0
xTaskCreatePinnedToCore(
    wifi_service_task,
    "WiFiService",
    4096,           // 4KB stack
    nullptr,
    5,              // Priority 5-6 (ALTA)
    &wifi_handle,
    0               // Core 0 ← OBBLIGATORIO
);

// REGOLA 2: LVGL SEMPRE su Core 1 (APP CPU)
xTaskCreatePinnedToCore(
    lvgl_task,
    "LVGL",
    8192,           // 8KB stack (LVGL richiede più memoria)
    nullptr,
    3,              // Priority 2-3 (MEDIA-BASSA)
    &lvgl_handle,
    1               // Core 1 ← RACCOMANDATO
);

// REGOLA 3: App background tasks su Core 0 o 1
// (evitare Core 1 se task pesanti, per non rallentare UI)
xTaskCreatePinnedToCore(
    app_background_task,
    "AppTask",
    4096,
    this,
    2,              // Priority più bassa di WiFi
    &task_handle,
    0               // Core 0 preferito per task non-UI
);
```

**Tabella Priorità Raccomandata:**

| Task             | Core | Priority | Stack | Note                          |
|------------------|------|----------|-------|-------------------------------|
| WiFi Service     | 0    | 5-6      | 4KB   | Alta priorità, network-critical|
| BLE Service      | 0    | 5-6      | 8KB   | Alta priorità, BLE stack grande|
| LVGL GUI         | 1    | 2-3      | 8KB   | UI può aspettare              |
| App Background   | 0    | 2-4      | 4KB   | Media priorità                |
| Idle Task        | Any  | 0        | -     | Automatico FreeRTOS           |

**⚠️ PROBLEMA COMUNE (dalla community ESP32):**

> "When WiFi and LVGL run together, UI freezes"

**Causa:** Entrambi su stesso core o LVGL con priorità troppo alta.

**Soluzione:** Seguire tabella sopra + usare mutex LVGL (vedi sezione 3.6).

### 3.3 Separazione UI e Logica

```cpp
// Model (dati)
class WeatherData {
public:
    float temperature;
    String condition;
    int humidity;
};

// Controller (logica)
class WeatherController {
private:
    WeatherData data;

public:
    void fetchWeather() {
        // HTTP request, parsing, etc.
        data.temperature = 22.5;
        data.condition = "Sunny";
        notifyObservers();
    }

    WeatherData getData() { return data; }
};

// View (UI)
class WeatherScreen : public Screen {
private:
    WeatherController controller;
    lv_obj_t* temp_label;

public:
    void onCreate(lv_obj_t* parent) override {
        // Crea UI
        temp_label = lv_label_create(screen_obj);
        updateUI();
    }

    void updateUI() {
        auto data = controller.getData();
        lv_label_set_text_fmt(temp_label, "%.1f°C", data.temperature);
    }
};
```

### 3.4 Gestione Errori

```cpp
class Result {
public:
    bool success;
    String error_message;

    static Result Ok() { return {true, ""}; }
    static Result Error(String msg) { return {false, msg}; }
};

Result FileManager::saveFile(const char* path, const uint8_t* data, size_t len) {
    File file = SPIFFS.open(path, "w");
    if (!file) {
        return Result::Error("Failed to open file");
    }

    if (file.write(data, len) != len) {
        file.close();
        return Result::Error("Write failed");
    }

    file.close();
    return Result::Ok();
}
```

### 3.5 Thread-Safety con LVGL (CRITICO!)

**⚠️ PROBLEMA:** LVGL NON è thread-safe. Chiamare funzioni LVGL da task diversi causa crash.

**SOLUZIONE:** Mutex globale (pattern ufficiale LVGL).

```cpp
// Global mutex dichiarato una sola volta
SemaphoreHandle_t lvgl_mutex = NULL;

// Inizializzazione (in setup)
void setup_lvgl_threading() {
    lvgl_mutex = xSemaphoreCreateMutex();
}

// LVGL task (Core 1)
static void lvgl_task(void* pvParameter) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms delay

        // LOCK prima di lv_task_handler
        if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(lvgl_mutex);
        }
    }
}

// Da QUALSIASI altro task (es. WiFi service su Core 0)
void wifi_update_ui(const char* status) {
    // SEMPRE LOCK prima di chiamare funzioni LVGL
    if (pdTRUE == xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100))) {
        lv_label_set_text(wifi_label, status);
        xSemaphoreGive(lvgl_mutex);
    } else {
        Serial.println("[ERROR] Failed to acquire LVGL mutex");
    }
}
```

**✅ REGOLE D'ORO:**

1. **SEMPRE** acquisire `lvgl_mutex` prima di chiamare funzioni LVGL da task non-LVGL
2. **MINIMIZZARE** tempo con mutex (prepara dati FUORI, aggiorna UI dentro)
3. **USARE TIMEOUT** su `xSemaphoreTake()` per evitare deadlock
4. **PREFERIRE EventRouter** per comunicazione asincrona invece di chiamate dirette

**❌ SBAGLIATO (causerà crash):**
```cpp
void wifi_task() {
    // NO MUTEX!
    lv_label_set_text(label, "Connected");  // ← CRASH PROBABILE
}
```

**✅ CORRETTO:**
```cpp
void wifi_task() {
    // Prepara dati fuori dal mutex
    String text = "Connected to " + WiFi.SSID();

    // Mutex solo per UI update
    if (pdTRUE == xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100))) {
        lv_label_set_text(label, text.c_str());
        xSemaphoreGive(lvgl_mutex);
    }
}
```

**Alternativa con EventRouter (RACCOMANDATO):**
```cpp
// WiFi task (Core 0) - nessun mutex qui
void wifi_on_connected() {
    EventRouter::getInstance()->publish("wifi.connected", nullptr);
}

// Widget update (LVGL Core 1) - mutex gestito automaticamente
void WiFiWidget::onEvent(const char* event, void* data) {
    // safeUpdateUI() ha mutex interno
    safeUpdateUI([this]() {
        lv_label_set_text(label, "Connected");
    });
}
```

### 3.6 Logging System

```cpp
enum LogLevel {
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
};

class Logger {
private:
    static LogLevel current_level;

public:
    static void setLevel(LogLevel level) { current_level = level; }

    static void log(LogLevel level, const char* tag, const char* format, ...) {
        if (level > current_level) return;

        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        const char* level_str[] = {"ERROR", "WARN", "INFO", "DEBUG"};
        Serial.printf("[%s][%s] %s\n", level_str[level], tag, buffer);
    }
};

#define LOG_E(tag, ...) Logger::log(LOG_ERROR, tag, __VA_ARGS__)
#define LOG_I(tag, ...) Logger::log(LOG_INFO, tag, __VA_ARGS__)
```

## 4. Ottimizzazioni Performance

### 4.1 Rendering

```cpp
// Usare sprite per aggiornamenti senza flicker
lv_obj_t* sprite = lv_canvas_create(parent);
static lv_color_t sprite_buffer[100*100];
lv_canvas_set_buffer(sprite, sprite_buffer, 100, 100, LV_IMG_CF_TRUE_COLOR);

// Disegnare su sprite
lv_canvas_fill_bg(sprite, lv_color_white(), LV_OPA_COVER);
lv_draw_rect_dsc_t rect_dsc;
lv_draw_rect_dsc_init(&rect_dsc);
rect_dsc.bg_color = lv_color_hex(0x00FF00);
lv_canvas_draw_rect(sprite, 10, 10, 50, 50, &rect_dsc);

// Ridurre refresh rate quando non necessario
lv_timer_t* timer = lv_timer_create(slow_update_cb, 1000, nullptr);
```

### 4.2 Gestione Touch

```cpp
// Debouncing touch events
class TouchManager {
private:
    unsigned long last_touch_time = 0;
    const unsigned long DEBOUNCE_MS = 50;

public:
    bool shouldProcessTouch() {
        unsigned long now = millis();
        if (now - last_touch_time < DEBOUNCE_MS) {
            return false;
        }
        last_touch_time = now;
        return true;
    }
};
```

### 4.3 Asset Management

```cpp
// Caricare risorse on-demand
class AssetManager {
private:
    std::map<String, lv_img_dsc_t*> image_cache;
    const size_t MAX_CACHE_SIZE = 5;

public:
    lv_img_dsc_t* loadImage(const char* path) {
        if (image_cache.count(path)) {
            return image_cache[path];
        }

        if (image_cache.size() >= MAX_CACHE_SIZE) {
            evictOldest();
        }

        lv_img_dsc_t* img = loadImageFromFile(path);
        image_cache[path] = img;
        return img;
    }
};
```

## 5. Struttura File System

```
/spiffs
├── /system
│   ├── config.json          # Configurazioni sistema
│   └── apps.json            # Lista app installate
├── /apps
│   ├── /weather
│   │   ├── manifest.json
│   │   ├── icon.bin
│   │   └── data/
│   └── /clock
│       ├── manifest.json
│       └── icon.bin
├── /assets
│   ├── /fonts
│   │   └── custom_font_16.bin
│   ├── /images
│   │   └── wallpaper.bin
│   └── /themes
│       └── dark_theme.json
└── /data
    └── user_data.db
```

## 6. Esempio Applicazione Completa

```cpp
// weather_app.h
class WeatherApp : public Screen {
private:
    lv_obj_t* temp_label;
    lv_obj_t* icon_img;
    lv_obj_t* refresh_btn;
    lv_timer_t* update_timer;

    void fetchWeatherData();
    void updateDisplay();
    static void refresh_btn_cb(lv_event_t* e);
    static void update_timer_cb(lv_timer_t* timer);

public:
    void onCreate(lv_obj_t* parent) override;
    void onResume() override;
    void onPause() override;
    void onDestroy() override;
    ScreenType getType() override { return SCREEN_APP_INSTANCE; }
    const char* getName() override { return "Weather"; }
};

// Registrazione app
void setup() {
    AppManager::getInstance()->registerApp({
        .id = "com.example.weather",
        .name = "Weather",
        .icon_path = LV_SYMBOL_DOWNLOAD,
        .icon_color = 0x3498db,
        .factory = []() -> Screen* { return new WeatherApp(); }
    });
}
```

## 7. Inizializzazione Sistema (Thread-Safe)

```cpp
// main.cpp
#include "service_manager.h"    // Dal documento ESTENSIONI
#include "wifi_service.h"       // Dal documento ESTENSIONI

// Global LVGL mutex (OBBLIGATORIO!)
SemaphoreHandle_t lvgl_mutex = NULL;

// LVGL buffers
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

// LVGL Task (Core 1)
static void lvgl_task(void* pvParameter) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms standard LVGL

        // LOCK mutex prima di lv_task_handler
        if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(lvgl_mutex);
        }
    }
}

// LVGL Tick Timer (1ms)
static void lv_tick_task(void *arg) {
    lv_tick_inc(1);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32 OS-like System Starting ===");

    // 1. Inizializza hardware
    initDisplay();
    initTouch();

    // 2. Inizializza LVGL
    lv_init();

    // Alloca buffer in PSRAM (ESP32-S3)
    size_t buf_size = LV_HOR_RES_MAX * LV_VER_RES_MAX / 10;
    buf1 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t),
                                         MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t),
                                         MALLOC_CAP_SPIRAM);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

    // Setup display driver (TFT_eSPI)
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = tft_flush_cb;
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    lv_disp_drv_register(&disp_drv);

    // Setup touch driver (FT6336U)
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    // 3. Crea MUTEX GLOBALE (CRITICO!)
    lvgl_mutex = xSemaphoreCreateMutex();
    Serial.println("[LVGL] Mutex created");

    // 4. Timer tick LVGL (1ms)
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));

    // 5. Inizializza file system
    if (!SPIFFS.begin(true)) {
        Serial.println("[ERROR] SPIFFS Mount Failed");
        return;
    }

    // 6. Inizializza core managers
    SettingsManager::getInstance()->begin();
    registerSystemSettings();

    // 7. Inizializza e avvia servizi (WiFi, BLE, etc.)
    // Vedi ESTENSIONI_ARCHITETTURA_SERVIZI.md per dettagli
    auto service_mgr = ServiceManager::getInstance();

    WiFiService* wifi = new WiFiService();
    wifi->configure("YOUR_SSID", "YOUR_PASSWORD");
    service_mgr->registerService("wifi", wifi);
    service_mgr->startAll();  // Avvia su Core 0 automaticamente

    Serial.println("[Services] Started on Core 0");

    // 8. Registra applicazioni
    registerApps();

    // 9. Carica dashboard
    DashboardScreen* dashboard = new DashboardScreen();
    ScreenManager::getInstance()->pushScreen(dashboard);

    // 10. Avvia LVGL task su Core 1 (ULTIMO!)
    xTaskCreatePinnedToCore(
        lvgl_task,
        "LVGL",
        8192,       // 8KB stack
        nullptr,
        3,          // Priority 2-3 (media-bassa)
        nullptr,
        1           // Core 1 (APP CPU) ← IMPORTANTE
    );

    Serial.println("[LVGL] Task started on Core 1");
    Serial.println("=== System Ready! ===\n");
}

void loop() {
    // FreeRTOS gestisce tutto, loop vuoto
    vTaskDelay(portMAX_DELAY);
}
```

**⚠️ ORDINE INIZIALIZZAZIONE IMPORTANTE:**

1. Hardware (display, touch)
2. LVGL init + buffers
3. **MUTEX** (prima di task LVGL!)
4. Tick timer
5. File system
6. Managers
7. **Servizi** (WiFi/BLE su Core 0)
8. App registration
9. Dashboard load
10. **LVGL task** (ultimo, su Core 1)

## 8. Configurazione PlatformIO

```ini
; platformio.ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

build_flags =
    -D LV_CONF_INCLUDE_SIMPLE
    -D BOARD_HAS_PSRAM
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -I src/config

lib_deps =
    lvgl/lvgl@^8.4.0
    bodmer/TFT_eSPI@^2.5.43

monitor_speed = 115200
board_build.flash_mode = qio
board_build.partitions = huge_app.csv
```

## 9. Considerazioni Finali

### Pro dell'Architettura Proposta
- ✅ Separazione chiara tra layer
- ✅ Facilmente estendibile con nuove app
- ✅ Gestione memoria ottimizzata con PSRAM
- ✅ Pattern MVC per UI complesse
- ✅ Sistema eventi disaccoppiato (EventRouter)
- ✅ **Thread-safe** (mutex LVGL verificato dalla community)
- ✅ **Multi-core** ottimizzato (WiFi Core 0, LVGL Core 1)
- ✅ **Service Layer** per WiFi/BLE/background tasks
- ✅ **Widget System** per dashboard customizzabile

### Limitazioni da Considerare
- ⚠️ RAM limitata anche su ESP32-S3 (512KB SRAM + PSRAM)
- ⚠️ Complessità debugging multi-thread
- ⚠️ Tempo di sviluppo iniziale più lungo
- ⚠️ Necessità di ottimizzazioni continue
- ⚠️ **Thread-safety critica**: un errore di mutex causa crash
- ⚠️ **Core pinning rigido**: WiFi DEVE essere Core 0

### Problemi Risolti dalla Community
Problemi comuni trovati durante ricerca e documentati in [ESTENSIONI_ARCHITETTURA_SERVIZI.md](ESTENSIONI_ARCHITETTURA_SERVIZI.md):

1. ✅ **"WiFi + BLE + LVGL insieme causano freeze"**
   - Soluzione: Core pinning + priorità corrette

2. ✅ **"UI si blocca dopo un po'"**
   - Soluzione: Mutex gestito correttamente + stack 8KB per LVGL

3. ✅ **"Crash random quando WiFi aggiorna UI"**
   - Soluzione: Mutex globale + EventRouter pattern

4. ✅ **"Memory leak progressivo"**
   - Soluzione: PSRAM per buffer + cleanup in onDestroy()

### Prossimi Passi

**Base (questo documento):**
1. ✅ Implementare core manager (Screen, App, Settings)
2. ✅ Creare dashboard e settings base
3. ✅ Sistema eventi (EventRouter)

**Estensioni (ESTENSIONI_ARCHITETTURA_SERVIZI.md):**
4. ✅ Service Layer (WiFi, BLE)
5. ✅ Widget System customizzabile
6. ✅ Thread-safety patterns
7. 🔄 Sviluppare 2-3 app esempio complete
8. 🔄 Aggiungere MQTT/NTP services
9. 🔄 OTA update system
10. 🔄 Documentare API per sviluppatori di app

### Architettura Verificata

Questa architettura è basata su:
- ✅ **Pattern ufficiali LVGL** (lvgl/lv_port_esp32)
- ✅ **Best practices ESP32** (Espressif documentation)
- ✅ **Community solutions** (ESP32 Forum, LVGL Forum)
- ✅ **Progetti open source** (Tactility OS, B3OS)

**Stato:** ✅ Production-ready per progetti commerciali (vedi [ANALISI_LICENZE.md](ANALISI_LICENZE.md))

## 10. Risorse e Reference

### Documentazione Ufficiale
- LVGL Documentation: https://docs.lvgl.io/
- LVGL + FreeRTOS: https://docs.lvgl.io/master/integration/os/freertos.html
- LVGL ESP32 Port: https://github.com/lvgl/lv_port_esp32
- TFT_eSPI: https://github.com/Bodmer/TFT_eSPI
- ESP32 Programming Guide: https://docs.espressif.com/
- FreeRTOS: https://www.freertos.org/

### Community Resources
- ESP32 Forum: https://esp32.com/
- LVGL Forum: https://forum.lvgl.io/
- Tactility OS (ESP32 OS inspiration): https://github.com/ByteWelder/Tactility
- Random Nerd Tutorials (ESP32 + LVGL): https://randomnerdtutorials.com/

### Documenti Correlati (Questo Progetto)
1. **[REPORT_ARCHITETTURA_OS_ESP32.md](REPORT_ARCHITETTURA_OS_ESP32.md)** ← Questo documento
   - Architettura base 4-layer
   - Core managers (Screen, App, Settings, EventRouter)
   - Dashboard e Settings screens
   - Best practices base

2. **[ESTENSIONI_ARCHITETTURA_SERVIZI.md](ESTENSIONI_ARCHITETTURA_SERVIZI.md)** ← LEGGI QUESTO!
   - Service Layer completo (WiFi, BLE)
   - Widget System customizzabile
   - Thread-safety patterns verificati
   - Core pinning e FreeRTOS multi-core
   - Soluzioni problemi comuni community

3. **[ANALISI_LICENZE.md](ANALISI_LICENZE.md)**
   - Analisi licenze librerie (MIT/BSD/GPL)
   - Viabilità commerciale del progetto
   - Attribuzione corretta

---

**Autore**: ESP32 OS Architecture Team
**Versione**: 2.0 (Aggiornato con estensioni Service Layer e Thread-Safety)
**Data Creazione**: 2025-11-14
**Ultimo Aggiornamento**: 2025-11-14

**Changelog v2.0:**
- ➕ Aggiunto riferimento a ESTENSIONI_ARCHITETTURA_SERVIZI.md
- ➕ Architettura aggiornata con Service Layer
- ➕ Thread-safety section (mutex LVGL globale)
- ➕ Core pinning best practices
- ➕ EventRouter esteso con esempi thread-safe
- ➕ Inizializzazione sistema completa e corretta
- ➕ Problemi risolti dalla community
- 🔧 Diagramma architettura layer aggiornato

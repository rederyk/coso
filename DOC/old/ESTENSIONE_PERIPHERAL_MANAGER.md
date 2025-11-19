# Estensione Architettura: Peripheral Manager Layer

> **⚠️ NOTA IMPORTANTE:** Questo documento descrive l'**architettura TARGET** del Peripheral Manager Layer.
> Per lo **stato REALE attuale** del progetto e la roadmap di implementazione, consultare **[STATO_ATTUALE_PROGETTO.md](STATO_ATTUALE_PROGETTO.md)**

## Executive Summary

Questo documento estende [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) aggiungendo un **Peripheral Manager Layer** per gestire in modo modulare e thread-safe tutte le periferiche hardware (GPIO, I2C, SPI, ADC, PWM, UART).

**Obiettivi:**
- 📋 Gestione centralizzata delle risorse hardware
- 📋 Prevenzione conflitti (stesso pin usato da 2 app)
- 📋 Hot-plug detection per periferiche I2C/SPI
- 📋 API uniforme per tutte le periferiche
- 📋 Thread-safe e compatibile con FreeRTOS multi-core
- 📋 Event-driven (notifiche via EventRouter)

**📌 Status:** PIANIFICATO - Da implementare in Fase 6 della roadmap (vedi STATO_ATTUALE_PROGETTO.md)

---

## 1. Architettura Estesa

### 1.1 Nuovo Layer nella Stack

```
┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                            │
│  (Apps possono richiedere periferiche al PeripheralManager)     │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│              SYSTEM SERVICES LAYER                               │
│  (WiFi, BLE, NTP usano periferiche tramite PeripheralManager)   │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│              PERIPHERAL MANAGER LAYER ← NEW!                     │
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │              PeripheralManager (Registry)                   │ │
│  │  - Resource allocation/deallocation                         │ │
│  │  - Conflict detection                                       │ │
│  │  - Event publishing (peripheral events)                     │ │
│  └────────────────────────────────────────────────────────────┘ │
│                             │                                     │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┐       │
│  │          │          │          │          │          │       │
│  ▼          ▼          ▼          ▼          ▼          ▼       │
│  GPIO       I2C        SPI        ADC        PWM       UART      │
│  Manager    Manager    Manager    Manager    Manager   Manager  │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│                   CORE MANAGERS LAYER                            │
│  (Screen, App, Settings, EventRouter)                            │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│                   GUI FRAMEWORK LAYER (LVGL)                     │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│              HARDWARE ABSTRACTION LAYER                          │
│  (TFT_eSPI, FT6336U, ESP32 HAL)                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Peripheral Manager - Core Architecture

### 2.1 Peripheral Base Class

```cpp
// peripheral_base.h
#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>

enum PeripheralType {
    PERIPH_GPIO,
    PERIPH_I2C,
    PERIPH_SPI,
    PERIPH_ADC,
    PERIPH_PWM,
    PERIPH_UART,
    PERIPH_CUSTOM
};

enum PeripheralState {
    PERIPH_UNINITIALIZED,
    PERIPH_INITIALIZED,
    PERIPH_ALLOCATED,     // In uso da qualcuno
    PERIPH_ERROR
};

struct PeripheralConfig {
    const char* name;
    PeripheralType type;
    uint8_t pin_or_id;    // GPIO pin o I2C bus ID, ecc.
    void* custom_config;  // Config specifica del tipo
};

class Peripheral {
protected:
    const char* name = "Unknown";
    PeripheralType type;
    PeripheralState state = PERIPH_UNINITIALIZED;
    const char* owner = nullptr;  // Chi sta usando la periferica

    // EventRouter per notifiche
    EventRouter* event_router = nullptr;

public:
    Peripheral(PeripheralType t) : type(t) {
        event_router = EventRouter::getInstance();
    }

    virtual ~Peripheral() {}

    // Override questi metodi
    virtual bool init(PeripheralConfig config) = 0;
    virtual void deinit() = 0;
    virtual bool isAvailable() const = 0;

    // Allocazione (chi sta usando questa periferica?)
    virtual bool allocate(const char* owner_name) {
        if (state == PERIPH_ALLOCATED) {
            Serial.printf("[Peripheral] %s already allocated by %s\n", name, owner);
            return false;
        }

        owner = owner_name;
        state = PERIPH_ALLOCATED;
        Serial.printf("[Peripheral] %s allocated by %s\n", name, owner_name);

        // Pubblica evento
        event_router->publish("peripheral.allocated", (void*)name);
        return true;
    }

    virtual void deallocate() {
        if (state == PERIPH_ALLOCATED) {
            Serial.printf("[Peripheral] %s deallocated by %s\n", name, owner);
            owner = nullptr;
            state = PERIPH_INITIALIZED;
            event_router->publish("peripheral.deallocated", (void*)name);
        }
    }

    // Getters
    const char* getName() const { return name; }
    PeripheralType getType() const { return type; }
    PeripheralState getState() const { return state; }
    const char* getOwner() const { return owner; }
    bool isAllocated() const { return state == PERIPH_ALLOCATED; }
};
```

---

## 3. GPIO Manager

### 3.1 GPIO Peripheral Implementation

```cpp
// gpio_manager.h
#pragma once
#include "peripheral_base.h"
#include <map>

enum GPIOMode {
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_INPUT_PULLUP,
    GPIO_MODE_INPUT_PULLDOWN
};

struct GPIOConfig {
    uint8_t pin;
    GPIOMode mode;
    bool initial_state;  // Per output
};

class GPIOPeripheral : public Peripheral {
private:
    uint8_t pin;
    GPIOMode mode;
    bool interrupt_enabled = false;
    std::function<void()> isr_callback = nullptr;

    static void IRAM_ATTR handleISR(void* arg) {
        GPIOPeripheral* self = (GPIOPeripheral*)arg;
        if (self->isr_callback) {
            self->isr_callback();
        }
    }

public:
    GPIOPeripheral(uint8_t gpio_pin) : Peripheral(PERIPH_GPIO), pin(gpio_pin) {
        name = ("GPIO_" + String(gpio_pin)).c_str();
    }

    bool init(PeripheralConfig config) override {
        GPIOConfig* gpio_cfg = (GPIOConfig*)config.custom_config;

        pin = gpio_cfg->pin;
        mode = gpio_cfg->mode;

        // Configura pin
        switch (mode) {
            case GPIO_MODE_INPUT:
                pinMode(pin, INPUT);
                break;
            case GPIO_MODE_OUTPUT:
                pinMode(pin, OUTPUT);
                digitalWrite(pin, gpio_cfg->initial_state);
                break;
            case GPIO_MODE_INPUT_PULLUP:
                pinMode(pin, INPUT_PULLUP);
                break;
            case GPIO_MODE_INPUT_PULLDOWN:
                pinMode(pin, INPUT_PULLDOWN);
                break;
        }

        state = PERIPH_INITIALIZED;
        Serial.printf("[GPIO] Pin %d initialized as %s\n",
                      pin, mode == GPIO_MODE_OUTPUT ? "OUTPUT" : "INPUT");
        return true;
    }

    void deinit() override {
        if (interrupt_enabled) {
            detachInterrupt(digitalPinToInterrupt(pin));
            interrupt_enabled = false;
        }
        state = PERIPH_UNINITIALIZED;
    }

    bool isAvailable() const override {
        return state != PERIPH_ERROR;
    }

    // GPIO Operations
    void write(bool value) {
        if (mode == GPIO_MODE_OUTPUT) {
            digitalWrite(pin, value);
        }
    }

    bool read() {
        return digitalRead(pin);
    }

    void toggle() {
        if (mode == GPIO_MODE_OUTPUT) {
            digitalWrite(pin, !digitalRead(pin));
        }
    }

    // Interrupt support
    void attachInterrupt(std::function<void()> callback, int mode_int) {
        isr_callback = callback;
        ::attachInterruptArg(digitalPinToInterrupt(pin), handleISR, this, mode_int);
        interrupt_enabled = true;
        Serial.printf("[GPIO] Interrupt attached to pin %d\n", pin);
    }

    void detachInterrupt() {
        if (interrupt_enabled) {
            ::detachInterrupt(digitalPinToInterrupt(pin));
            interrupt_enabled = false;
            isr_callback = nullptr;
        }
    }

    uint8_t getPin() const { return pin; }
};

// GPIO Manager - Gestisce tutti i GPIO
class GPIOManager {
private:
    static GPIOManager* instance;
    std::map<uint8_t, GPIOPeripheral*> gpio_map;
    SemaphoreHandle_t mutex;

    GPIOManager() {
        mutex = xSemaphoreCreateMutex();
    }

public:
    static GPIOManager* getInstance() {
        if (instance == nullptr) {
            instance = new GPIOManager();
        }
        return instance;
    }

    // Richiedi GPIO (con allocazione automatica)
    GPIOPeripheral* requestGPIO(uint8_t pin, GPIOMode mode,
                                const char* owner, bool initial_state = LOW) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return nullptr;
        }

        // Controlla se già esiste
        if (gpio_map.find(pin) != gpio_map.end()) {
            GPIOPeripheral* existing = gpio_map[pin];
            if (existing->isAllocated()) {
                Serial.printf("[GPIO] Pin %d already allocated by %s\n",
                              pin, existing->getOwner());
                xSemaphoreGive(mutex);
                return nullptr;
            }

            // Rialloca
            existing->allocate(owner);
            xSemaphoreGive(mutex);
            return existing;
        }

        // Crea nuovo GPIO
        GPIOPeripheral* gpio = new GPIOPeripheral(pin);

        PeripheralConfig config;
        GPIOConfig gpio_cfg = {pin, mode, initial_state};
        config.custom_config = &gpio_cfg;

        if (!gpio->init(config)) {
            delete gpio;
            xSemaphoreGive(mutex);
            return nullptr;
        }

        gpio->allocate(owner);
        gpio_map[pin] = gpio;

        xSemaphoreGive(mutex);
        return gpio;
    }

    // Rilascia GPIO
    void releaseGPIO(uint8_t pin, const char* owner) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }

        if (gpio_map.find(pin) != gpio_map.end()) {
            GPIOPeripheral* gpio = gpio_map[pin];

            // Verifica ownership
            if (strcmp(gpio->getOwner(), owner) == 0) {
                gpio->deallocate();
            } else {
                Serial.printf("[GPIO] Pin %d not owned by %s (owned by %s)\n",
                              pin, owner, gpio->getOwner());
            }
        }

        xSemaphoreGive(mutex);
    }

    // Query
    bool isPinAvailable(uint8_t pin) {
        if (gpio_map.find(pin) == gpio_map.end()) {
            return true;  // Non ancora usato
        }
        return !gpio_map[pin]->isAllocated();
    }

    void printStatus() {
        Serial.println("\n=== GPIO Manager Status ===");
        for (auto& pair : gpio_map) {
            GPIOPeripheral* gpio = pair.second;
            Serial.printf("  GPIO %d: %s (Owner: %s)\n",
                          pair.first,
                          gpio->isAllocated() ? "ALLOCATED" : "FREE",
                          gpio->getOwner() ? gpio->getOwner() : "None");
        }
        Serial.println("===========================\n");
    }
};

GPIOManager* GPIOManager::instance = nullptr;
```

---

## 4. I2C Manager

### 4.1 I2C Bus Management

```cpp
// i2c_manager.h
#pragma once
#include "peripheral_base.h"
#include <Wire.h>
#include <map>
#include <vector>

struct I2CConfig {
    uint8_t bus_id;    // 0 o 1 (ESP32 ha 2 bus I2C)
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint32_t frequency;
};

struct I2CDevice {
    uint8_t address;
    const char* name;
    const char* owner;
    bool responding;
};

class I2CPeripheral : public Peripheral {
private:
    uint8_t bus_id;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint32_t frequency;
    TwoWire* wire = nullptr;
    std::map<uint8_t, I2CDevice> devices;  // address -> device
    SemaphoreHandle_t bus_mutex;

public:
    I2CPeripheral(uint8_t bus) : Peripheral(PERIPH_I2C), bus_id(bus) {
        name = ("I2C_" + String(bus)).c_str();
        bus_mutex = xSemaphoreCreateMutex();
    }

    bool init(PeripheralConfig config) override {
        I2CConfig* i2c_cfg = (I2CConfig*)config.custom_config;

        sda_pin = i2c_cfg->sda_pin;
        scl_pin = i2c_cfg->scl_pin;
        frequency = i2c_cfg->frequency;

        // Seleziona bus
        if (bus_id == 0) {
            wire = &Wire;
        } else if (bus_id == 1) {
            wire = &Wire1;
        } else {
            return false;
        }

        // Inizializza I2C
        wire->begin(sda_pin, scl_pin, frequency);

        state = PERIPH_INITIALIZED;
        Serial.printf("[I2C] Bus %d initialized (SDA:%d, SCL:%d, %d Hz)\n",
                      bus_id, sda_pin, scl_pin, frequency);

        // Scan devices
        scanBus();

        return true;
    }

    void deinit() override {
        if (wire) {
            wire->end();
        }
        state = PERIPH_UNINITIALIZED;
    }

    bool isAvailable() const override {
        return state != PERIPH_ERROR && wire != nullptr;
    }

    // Scan I2C bus per device
    void scanBus() {
        if (!wire) return;

        Serial.printf("[I2C] Scanning bus %d...\n", bus_id);
        devices.clear();

        for (uint8_t addr = 1; addr < 127; addr++) {
            wire->beginTransmission(addr);
            uint8_t error = wire->endTransmission();

            if (error == 0) {
                Serial.printf("  [I2C] Device found at 0x%02X\n", addr);

                I2CDevice dev;
                dev.address = addr;
                dev.name = "Unknown";
                dev.owner = nullptr;
                dev.responding = true;
                devices[addr] = dev;

                // Pubblica evento
                char addr_str[8];
                snprintf(addr_str, sizeof(addr_str), "0x%02X", addr);
                event_router->publish("i2c.device_found", (void*)addr_str);
            }
        }

        Serial.printf("[I2C] Scan complete. Found %d devices\n", devices.size());
    }

    // Alloca device specifico
    bool allocateDevice(uint8_t address, const char* owner_name,
                        const char* device_name = "Device") {
        if (devices.find(address) == devices.end()) {
            Serial.printf("[I2C] Device 0x%02X not found on bus %d\n",
                          address, bus_id);
            return false;
        }

        if (devices[address].owner != nullptr) {
            Serial.printf("[I2C] Device 0x%02X already allocated by %s\n",
                          address, devices[address].owner);
            return false;
        }

        devices[address].owner = owner_name;
        devices[address].name = device_name;
        Serial.printf("[I2C] Device 0x%02X (%s) allocated by %s\n",
                      address, device_name, owner_name);
        return true;
    }

    void deallocateDevice(uint8_t address, const char* owner_name) {
        if (devices.find(address) != devices.end()) {
            if (devices[address].owner == owner_name) {
                devices[address].owner = nullptr;
                Serial.printf("[I2C] Device 0x%02X deallocated\n", address);
            }
        }
    }

    // I2C Operations (thread-safe)
    bool write(uint8_t address, const uint8_t* data, size_t len) {
        if (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return false;
        }

        wire->beginTransmission(address);
        wire->write(data, len);
        uint8_t error = wire->endTransmission();

        xSemaphoreGive(bus_mutex);
        return (error == 0);
    }

    bool read(uint8_t address, uint8_t* buffer, size_t len) {
        if (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return false;
        }

        uint8_t received = wire->requestFrom(address, (uint8_t)len);
        if (received != len) {
            xSemaphoreGive(bus_mutex);
            return false;
        }

        for (size_t i = 0; i < len; i++) {
            buffer[i] = wire->read();
        }

        xSemaphoreGive(bus_mutex);
        return true;
    }

    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
        uint8_t data[2] = {reg, value};
        return write(address, data, 2);
    }

    bool readRegister(uint8_t address, uint8_t reg, uint8_t* value) {
        if (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return false;
        }

        wire->beginTransmission(address);
        wire->write(reg);
        uint8_t error = wire->endTransmission(false);

        if (error != 0) {
            xSemaphoreGive(bus_mutex);
            return false;
        }

        uint8_t received = wire->requestFrom(address, (uint8_t)1);
        if (received == 1) {
            *value = wire->read();
            xSemaphoreGive(bus_mutex);
            return true;
        }

        xSemaphoreGive(bus_mutex);
        return false;
    }

    // Query
    std::vector<uint8_t> getDeviceAddresses() {
        std::vector<uint8_t> addresses;
        for (auto& pair : devices) {
            addresses.push_back(pair.first);
        }
        return addresses;
    }

    bool isDeviceAvailable(uint8_t address) {
        return devices.find(address) != devices.end() &&
               devices[address].owner == nullptr;
    }

    TwoWire* getWire() { return wire; }

    void printStatus() {
        Serial.printf("\n=== I2C Bus %d Status ===\n", bus_id);
        for (auto& pair : devices) {
            I2CDevice& dev = pair.second;
            Serial.printf("  0x%02X: %s - %s (Owner: %s)\n",
                          dev.address,
                          dev.name,
                          dev.responding ? "Responding" : "Not responding",
                          dev.owner ? dev.owner : "None");
        }
        Serial.println("========================\n");
    }
};

// I2C Manager
class I2CManager {
private:
    static I2CManager* instance;
    std::map<uint8_t, I2CPeripheral*> buses;  // bus_id -> peripheral

    I2CManager() {}

public:
    static I2CManager* getInstance() {
        if (instance == nullptr) {
            instance = new I2CManager();
        }
        return instance;
    }

    // Inizializza bus I2C
    I2CPeripheral* initBus(uint8_t bus_id, uint8_t sda, uint8_t scl,
                           uint32_t freq = 100000) {
        if (buses.find(bus_id) != buses.end()) {
            Serial.printf("[I2C] Bus %d already initialized\n", bus_id);
            return buses[bus_id];
        }

        I2CPeripheral* i2c = new I2CPeripheral(bus_id);

        PeripheralConfig config;
        I2CConfig i2c_cfg = {bus_id, sda, scl, freq};
        config.custom_config = &i2c_cfg;

        if (!i2c->init(config)) {
            delete i2c;
            return nullptr;
        }

        buses[bus_id] = i2c;
        return i2c;
    }

    I2CPeripheral* getBus(uint8_t bus_id) {
        if (buses.find(bus_id) != buses.end()) {
            return buses[bus_id];
        }
        return nullptr;
    }

    void scanAllBuses() {
        for (auto& pair : buses) {
            pair.second->scanBus();
        }
    }

    void printStatus() {
        Serial.println("\n=== I2C Manager Status ===");
        for (auto& pair : buses) {
            pair.second->printStatus();
        }
        Serial.println("==========================\n");
    }
};

I2CManager* I2CManager::instance = nullptr;
```

---

## 5. Peripheral Manager - Registry Centralizzato

```cpp
// peripheral_manager.h
#pragma once
#include "peripheral_base.h"
#include "gpio_manager.h"
#include "i2c_manager.h"
#include <map>
#include <vector>

class PeripheralManager {
private:
    static PeripheralManager* instance;
    std::vector<Peripheral*> peripherals;
    SemaphoreHandle_t mutex;

    PeripheralManager() {
        mutex = xSemaphoreCreateMutex();
    }

public:
    static PeripheralManager* getInstance() {
        if (instance == nullptr) {
            instance = new PeripheralManager();
        }
        return instance;
    }

    // Inizializza tutti i manager
    void init() {
        Serial.println("[PeripheralMgr] Initializing...");

        // GPIO Manager già singleton
        // I2C Manager già singleton

        Serial.println("[PeripheralMgr] Initialized");
    }

    // Registra periferica custom
    void registerPeripheral(Peripheral* periph) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }

        peripherals.push_back(periph);
        Serial.printf("[PeripheralMgr] Registered %s\n", periph->getName());

        xSemaphoreGive(mutex);
    }

    // Trova periferiche per tipo
    std::vector<Peripheral*> findByType(PeripheralType type) {
        std::vector<Peripheral*> result;

        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return result;
        }

        for (auto* periph : peripherals) {
            if (periph->getType() == type) {
                result.push_back(periph);
            }
        }

        xSemaphoreGive(mutex);
        return result;
    }

    // Trova periferica per nome
    Peripheral* findByName(const char* name) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return nullptr;
        }

        for (auto* periph : peripherals) {
            if (strcmp(periph->getName(), name) == 0) {
                xSemaphoreGive(mutex);
                return periph;
            }
        }

        xSemaphoreGive(mutex);
        return nullptr;
    }

    // Get managers
    GPIOManager* getGPIOManager() {
        return GPIOManager::getInstance();
    }

    I2CManager* getI2CManager() {
        return I2CManager::getInstance();
    }

    // Status completo
    void printStatus() {
        Serial.println("\n========== PERIPHERAL MANAGER STATUS ==========");

        GPIOManager::getInstance()->printStatus();
        I2CManager::getInstance()->printStatus();

        Serial.println("  Custom Peripherals:");
        for (auto* periph : peripherals) {
            Serial.printf("    %s (%s) - %s\n",
                          periph->getName(),
                          periph->getType() == PERIPH_CUSTOM ? "Custom" : "Standard",
                          periph->isAllocated() ? "ALLOCATED" : "FREE");
        }

        Serial.println("===============================================\n");
    }
};

PeripheralManager* PeripheralManager::instance = nullptr;
```

---

## 6. Esempio di Utilizzo

### 6.1 Inizializzazione Sistema con Periferiche

```cpp
// main.cpp - setup() esteso
void setup() {
    // ... inizializzazioni precedenti ...

    // === AGGIUNGI DOPO Service Manager ===

    // Inizializza Peripheral Manager
    auto periph_mgr = PeripheralManager::getInstance();
    periph_mgr->init();

    // Inizializza I2C bus 0 (per touch e sensori)
    auto i2c_mgr = periph_mgr->getI2CManager();
    I2CPeripheral* i2c0 = i2c_mgr->initBus(0, 21, 22, 100000);  // SDA=21, SCL=22

    // Scan device I2C
    if (i2c0) {
        i2c0->scanBus();
    }

    // Alloca alcuni GPIO per LED/pulsanti
    auto gpio_mgr = periph_mgr->getGPIOManager();

    // LED di sistema su GPIO 2
    GPIOPeripheral* led_system = gpio_mgr->requestGPIO(2, GPIO_MODE_OUTPUT,
                                                        "system", LOW);
    if (led_system) {
        led_system->write(HIGH);  // Accendi LED
    }

    // Pulsante su GPIO 0
    GPIOPeripheral* btn = gpio_mgr->requestGPIO(0, GPIO_MODE_INPUT_PULLUP,
                                                 "button_handler");
    if (btn) {
        btn->attachInterrupt([]() {
            Serial.println("[Button] Pressed!");
            EventRouter::getInstance()->publish("button.pressed", nullptr);
        }, FALLING);
    }

    // Print status
    periph_mgr->printStatus();

    // ... continua con resto del setup ...
}
```

### 6.2 Esempio App che Usa GPIO

```cpp
// apps/led_controller_app.h
class LEDControllerApp : public Screen {
private:
    GPIOPeripheral* led_red = nullptr;
    GPIOPeripheral* led_green = nullptr;
    lv_obj_t* switch_red = nullptr;
    lv_obj_t* switch_green = nullptr;

public:
    void onCreate() override {
        auto gpio_mgr = PeripheralManager::getInstance()->getGPIOManager();

        // Richiedi GPIO
        led_red = gpio_mgr->requestGPIO(4, GPIO_MODE_OUTPUT,
                                        "LEDControllerApp", LOW);
        led_green = gpio_mgr->requestGPIO(5, GPIO_MODE_OUTPUT,
                                          "LEDControllerApp", LOW);

        if (!led_red || !led_green) {
            Serial.println("[LEDApp] Failed to allocate GPIO!");
            return;
        }

        // Crea UI
        screen = lv_obj_create(NULL);

        // Switch per LED rosso
        switch_red = lv_switch_create(screen);
        lv_obj_add_event_cb(switch_red, [](lv_event_t* e) {
            LEDControllerApp* self = (LEDControllerApp*)lv_event_get_user_data(e);
            bool state = lv_obj_has_state(self->switch_red, LV_STATE_CHECKED);
            self->led_red->write(state);
        }, LV_EVENT_VALUE_CHANGED, this);

        // Switch per LED verde
        switch_green = lv_switch_create(screen);
        lv_obj_add_event_cb(switch_green, [](lv_event_t* e) {
            LEDControllerApp* self = (LEDControllerApp*)lv_event_get_user_data(e);
            bool state = lv_obj_has_state(self->switch_green, LV_STATE_CHECKED);
            self->led_green->write(state);
        }, LV_EVENT_VALUE_CHANGED, this);
    }

    void onDestroy() override {
        // IMPORTANTE: Rilascia GPIO quando app chiude
        auto gpio_mgr = PeripheralManager::getInstance()->getGPIOManager();

        if (led_red) {
            gpio_mgr->releaseGPIO(4, "LEDControllerApp");
        }
        if (led_green) {
            gpio_mgr->releaseGPIO(5, "LEDControllerApp");
        }
    }
};
```

### 6.3 Esempio Service che Usa I2C

```cpp
// services/sensor_service.h
class SensorService : public SystemService {
private:
    I2CPeripheral* i2c_bus = nullptr;
    uint8_t sensor_address = 0x48;  // Esempio: TMP102 temperature sensor
    float temperature = 0.0f;

public:
    SensorService() {
        service_name = "SensorService";
        stack_size = 4096;
        priority = 4;
        core_id = 0;
    }

protected:
    bool onStart() override {
        // Ottieni bus I2C
        auto i2c_mgr = PeripheralManager::getInstance()->getI2CManager();
        i2c_bus = i2c_mgr->getBus(0);

        if (!i2c_bus) {
            Serial.println("[SensorService] I2C bus not available");
            return false;
        }

        // Alloca device
        if (!i2c_bus->allocateDevice(sensor_address, "SensorService", "TMP102")) {
            Serial.println("[SensorService] Failed to allocate sensor");
            return false;
        }

        Serial.println("[SensorService] Sensor allocated");
        return true;
    }

    void onStop() override {
        if (i2c_bus) {
            i2c_bus->deallocateDevice(sensor_address, "SensorService");
        }
    }

    void serviceMain() override {
        while (true) {
            // Leggi temperatura
            uint8_t temp_raw[2];
            if (i2c_bus->read(sensor_address, temp_raw, 2)) {
                temperature = ((temp_raw[0] << 4) | (temp_raw[1] >> 4)) * 0.0625;

                // Pubblica evento
                EventRouter::getInstance()->publish("sensor.temperature",
                                                    (void*)&temperature);
            }

            vTaskDelay(pdMS_TO_TICKS(5000));  // Ogni 5 secondi
        }
    }
};
```

---

## 7. Vantaggi dell'Architettura

✅ **Centralizzazione** - Un unico punto per gestire tutte le periferiche
✅ **Resource Management** - Previene conflitti (stesso pin usato da 2 app)
✅ **Thread-safe** - Mutex su ogni operazione critica
✅ **Hot-plug** - Scan I2C dinamico per rilevare device
✅ **Event-driven** - Notifiche via EventRouter per eventi hardware
✅ **Ownership tracking** - Sapere sempre chi sta usando una periferica
✅ **API uniforme** - Stesso pattern per GPIO, I2C, SPI, ecc.
✅ **Facilmente estendibile** - Aggiungere SPI/ADC/PWM/UART con stesso pattern

---

## 8. Manager Aggiuntivi (TODO)

### 8.1 SPI Manager (da implementare)

```cpp
// spi_manager.h - Struttura simile a I2C
class SPIManager {
    // Gestione bus SPI
    // CS pin allocation
    // DMA support
    // Transaction queueing
};
```

### 8.2 ADC Manager (da implementare)

```cpp
// adc_manager.h
class ADCManager {
    // Canali ADC
    // Attenuation settings
    // Continuous/one-shot mode
    // Calibration
};
```

### 8.3 PWM Manager (da implementare)

```cpp
// pwm_manager.h
class PWMManager {
    // LEDC channels
    // Frequency/duty cycle
    // Fade effects
};
```

### 8.4 UART Manager (da implementare)

```cpp
// uart_manager.h
class UARTManager {
    // Serial ports
    // Baud rate config
    // RX/TX buffer management
};
```

---

## 9. Integrazione con Architettura Esistente

### 9.1 Aggiornamento setup() Completo

```cpp
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32 OS Starting ===");

    // 1. Hardware base
    // initDisplay();
    // initTouch();

    // 2. LVGL
    // setup_lvgl();

    // 3. Peripheral Manager (NUOVO!)
    auto periph_mgr = PeripheralManager::getInstance();
    periph_mgr->init();

    // Inizializza I2C
    auto i2c_mgr = periph_mgr->getI2CManager();
    i2c_mgr->initBus(0, 21, 22, 100000);

    // Inizializza GPIO comuni
    auto gpio_mgr = periph_mgr->getGPIOManager();
    // ... allocazioni GPIO ...

    periph_mgr->printStatus();

    // 4. Core Managers
    SettingsManager::getInstance()->begin();

    // 5. Services (possono ora usare periferiche!)
    auto service_mgr = ServiceManager::getInstance();

    WiFiService* wifi = new WiFiService();
    wifi->configure("SSID", "PASSWORD");
    service_mgr->registerService("wifi", wifi);

    // Nuovo SensorService che usa I2C
    SensorService* sensor = new SensorService();
    service_mgr->registerService("sensor", sensor);

    service_mgr->startAll();

    // 6. Dashboard e apps
    // ...

    Serial.println("=== System Ready ===\n");
}
```

### 9.2 Aggiornamento Dashboard Widget

Puoi creare nuovi widget per periferiche:

```cpp
// widgets/gpio_monitor_widget.h
class GPIOMonitorWidget : public DashboardWidget {
    // Mostra stato GPIO allocati
    // Toggle LED
    // Stato pulsanti
};

// widgets/sensor_widget.h
class SensorWidget : public DashboardWidget {
    // Mostra temperatura/umidità da sensori I2C
    // Subscribe a eventi sensor.*
};
```

---

## 10. Conclusioni

### Modularità Verificata

Con l'aggiunta del **Peripheral Manager Layer**, l'architettura è ora **completamente modulare** per hardware:

```
✅ GPIO - Allocazione, interrupt, ownership tracking
✅ I2C - Multi-bus, device scan, transazioni thread-safe
🔄 SPI - Da implementare con stesso pattern
🔄 ADC - Da implementare con stesso pattern
🔄 PWM - Da implementare con stesso pattern
🔄 UART - Da implementare con stesso pattern
```

### Vantaggi Chiave

1. **Nessun conflitto hardware** - Sistema di allocazione previene uso multiplo
2. **App sicure** - App non possono "rubare" pin ad altre app
3. **Debug facile** - `printStatus()` mostra chi usa cosa
4. **Hot-plug ready** - Scan I2C dinamico per sensori
5. **Event-driven** - Eventi hardware pubblicati su EventRouter
6. **Estensibile** - Aggiungere nuove periferiche è standardizzato

---

**Documento:** ESTENSIONE_PERIPHERAL_MANAGER.md
**Versione:** 1.0
**Data:** 2025-11-14
**Estende:** [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)

**Prossimi Passi:**
1. Implementare SPI Manager
2. Implementare ADC Manager
3. Implementare PWM Manager (per LED dimming, motor control)
4. Implementare UART Manager (per debug, GPS, ecc.)
5. Creare widget dashboard per monitoring periferiche
6. Aggiungere app esempio che usa più periferiche (es. Home Automation Controller)

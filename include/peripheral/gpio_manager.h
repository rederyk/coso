// gpio_manager.h
// GPIO Manager per gestione pulsanti, LED e GPIO generici
// Supporta interrupt, debouncing e allocazione thread-safe

#pragma once
#include "peripheral_base.h"
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Modalità GPIO
enum GPIOMode {
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_INPUT_PULLUP,
    GPIO_MODE_INPUT_PULLDOWN
};

// Configurazione GPIO
struct GPIOConfig {
    uint8_t pin;
    GPIOMode mode;
    bool initial_state;  // Per output: HIGH/LOW iniziale
};

// Classe GPIOPeripheral - rappresenta un singolo pin GPIO
class GPIOPeripheral : public Peripheral {
private:
    uint8_t pin;
    GPIOMode mode;
    bool interrupt_enabled = false;
    std::function<void()> isr_callback = nullptr;

    // Buffer per nome dinamico
    char pin_name_buffer[16];

    // Handler ISR statico
    static void IRAM_ATTR handleISR(void* arg) {
        GPIOPeripheral* self = (GPIOPeripheral*)arg;
        if (self->isr_callback) {
            self->isr_callback();
        }
    }

public:
    GPIOPeripheral(uint8_t gpio_pin) : Peripheral(PERIPH_GPIO), pin(gpio_pin) {
        snprintf(pin_name_buffer, sizeof(pin_name_buffer), "GPIO_%d", gpio_pin);
        name = pin_name_buffer;
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

    // Operazioni GPIO
    void write(bool value) {
        if (mode == GPIO_MODE_OUTPUT && state == PERIPH_ALLOCATED) {
            digitalWrite(pin, value);
        }
    }

    bool read() {
        if (state == PERIPH_ALLOCATED) {
            return digitalRead(pin);
        }
        return false;
    }

    void toggle() {
        if (mode == GPIO_MODE_OUTPUT && state == PERIPH_ALLOCATED) {
            digitalWrite(pin, !digitalRead(pin));
        }
    }

    // Supporto interrupt
    void attachInterrupt(std::function<void()> callback, int mode_int) {
        if (state != PERIPH_ALLOCATED) {
            Serial.printf("[GPIO] Cannot attach interrupt: pin %d not allocated\n", pin);
            return;
        }

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
    GPIOMode getMode() const { return mode; }
};

// GPIO Manager - Gestisce tutti i GPIO con allocazione thread-safe
class GPIOManager {
private:
    static GPIOManager* instance;
    std::map<uint8_t, GPIOPeripheral*> gpio_map;
    SemaphoreHandle_t mutex;

    GPIOManager() {
        mutex = xSemaphoreCreateMutex();
        Serial.println("[GPIOManager] Initialized");
    }

public:
    static GPIOManager* getInstance() {
        if (instance == nullptr) {
            instance = new GPIOManager();
        }
        return instance;
    }

    // Richiedi GPIO (con allocazione automatica)
    // owner: nome dell'app/servizio che richiede il GPIO
    GPIOPeripheral* requestGPIO(uint8_t pin, GPIOMode mode,
                                const char* owner, bool initial_state = LOW) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            Serial.printf("[GPIOManager] Failed to acquire mutex\n");
            return nullptr;
        }

        // Controlla se già esiste
        if (gpio_map.find(pin) != gpio_map.end()) {
            GPIOPeripheral* existing = gpio_map[pin];
            if (existing->isAllocated()) {
                Serial.printf("[GPIOManager] Pin %d already allocated by %s\n",
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

        auto it = gpio_map.find(pin);
        if (it != gpio_map.end()) {
            GPIOPeripheral* gpio = it->second;

            // Verifica che sia il proprietario corretto
            if (gpio->getOwner() == owner || strcmp(gpio->getOwner(), owner) == 0) {
                gpio->deallocate();
            } else {
                Serial.printf("[GPIOManager] Release denied: pin %d owned by %s, not %s\n",
                              pin, gpio->getOwner(), owner);
            }
        }

        xSemaphoreGive(mutex);
    }

    // Query informazioni
    bool isPinAllocated(uint8_t pin) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return false;
        }

        bool allocated = false;
        auto it = gpio_map.find(pin);
        if (it != gpio_map.end()) {
            allocated = it->second->isAllocated();
        }

        xSemaphoreGive(mutex);
        return allocated;
    }

    // Stampa stato di tutti i GPIO
    void printStatus() {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
        }

        Serial.println("[GPIOManager] === GPIO Status ===");
        for (auto& pair : gpio_map) {
            GPIOPeripheral* gpio = pair.second;
            Serial.printf("  Pin %d: %s, Owner: %s\n",
                          gpio->getPin(),
                          gpio->isAllocated() ? "ALLOCATED" : "FREE",
                          gpio->getOwner() ? gpio->getOwner() : "none");
        }
        Serial.println("[GPIOManager] ==================");

        xSemaphoreGive(mutex);
    }
};

// Inizializza il singleton statico
GPIOManager* GPIOManager::instance = nullptr;

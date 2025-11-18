// peripheral_base.h
// Base class per tutte le periferiche hardware
// Parte dell'architettura Peripheral Manager Layer

#pragma once
#include <Arduino.h>
#include <functional>

// Tipi di periferiche supportate
enum PeripheralType {
    PERIPH_GPIO,
    PERIPH_I2C,
    PERIPH_SPI,
    PERIPH_ADC,
    PERIPH_PWM,
    PERIPH_UART,
    PERIPH_CUSTOM
};

// Stati possibili di una periferica
enum PeripheralState {
    PERIPH_UNINITIALIZED,  // Non ancora inizializzata
    PERIPH_INITIALIZED,    // Inizializzata ma non allocata
    PERIPH_ALLOCATED,      // In uso da un'applicazione
    PERIPH_ERROR          // Stato di errore
};

// Configurazione base per periferiche
struct PeripheralConfig {
    const char* name;
    PeripheralType type;
    uint8_t pin_or_id;    // GPIO pin o I2C bus ID, ecc.
    void* custom_config;  // Configurazione specifica del tipo
};

// Classe base astratta per tutte le periferiche
class Peripheral {
protected:
    const char* name = "Unknown";
    PeripheralType type;
    PeripheralState state = PERIPH_UNINITIALIZED;
    const char* owner = nullptr;  // Chi sta usando la periferica

public:
    Peripheral(PeripheralType t) : type(t) {}
    virtual ~Peripheral() {}

    // Metodi virtuali puri - devono essere implementati dalle classi derivate
    virtual bool init(PeripheralConfig config) = 0;
    virtual void deinit() = 0;
    virtual bool isAvailable() const = 0;

    // Allocazione risorse - chi sta usando questa periferica?
    virtual bool allocate(const char* owner_name) {
        if (state == PERIPH_ALLOCATED) {
            Serial.printf("[Peripheral] %s already allocated by %s\n", name, owner);
            return false;
        }

        owner = owner_name;
        state = PERIPH_ALLOCATED;
        Serial.printf("[Peripheral] %s allocated by %s\n", name, owner_name);
        return true;
    }

    virtual void deallocate() {
        if (state == PERIPH_ALLOCATED) {
            Serial.printf("[Peripheral] %s deallocated by %s\n", name, owner);
            owner = nullptr;
            state = PERIPH_INITIALIZED;
        }
    }

    // Getters
    const char* getName() const { return name; }
    PeripheralType getType() const { return type; }
    PeripheralState getState() const { return state; }
    const char* getOwner() const { return owner; }
    bool isAllocated() const { return state == PERIPH_ALLOCATED; }
};

#pragma once

#include <Arduino.h>
#include <array>
#include <cstdint>
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

/**
 * @brief Singleton class for managing the integrated RGB LED on ESP32-S3
 *
 * This driver manages the WS2812B RGB LED commonly integrated on ESP32-S3 dev boards
 * using the modern ESP-IDF RMT peripheral driver.
 */
class RgbLedManager {
public:
    enum class LedState {
        OFF,              // LED spento
        WIFI_CONNECTING,  // WiFi in connessione (blu lampeggiante)
        WIFI_CONNECTED,   // WiFi connesso (verde fisso)
        WIFI_ERROR,       // WiFi errore (rosso lampeggiante)
        BLE_ADVERTISING,  // BLE in advertising (ciano lampeggiante)
        BLE_CONNECTED,    // BLE connesso (ciano fisso)
        BOOT,             // Avvio (arcobaleno)
        ERROR,            // Errore generico (rosso fisso)
        CUSTOM,           // Colore personalizzato
        RAINBOW,          // Arcobaleno continuo
        STROBE,           // Lampeggio veloce (legacy)
        PULSE,            // Respiro lento (legacy)
        RGB_CYCLE,        // Ciclo RGB (legacy)
        PULSE_CUSTOM,     // Pulse con colore personalizzato
        STROBE_CUSTOM,    // Strobe con colore personalizzato
        PULSE_PALETTE     // Pulse con una palette di colori
    };

    static RgbLedManager& getInstance();
    ~RgbLedManager();

    /**
     * @brief Inizializza il driver del LED RGB
     * @param pin GPIO pin del LED RGB (default GPIO42 per Freenove ESP32-S3)
     * @return true se l'inizializzazione è riuscita
     */
    bool begin(uint8_t pin = 42);

    /**
     * @brief Imposta la luminosità generale del LED
     * @param brightness Valore 0-100 (percentuale)
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Ottiene la luminosità attuale
     * @return Valore 0-100
     */
    uint8_t getBrightness() const { return brightness_; }

    /**
     * @brief Imposta lo stato del LED
     * @param state Stato predefinito da visualizzare
     * @param temporary Se true, tornerà allo stato precedente dopo il timeout di inattività
     */
    void setState(LedState state, bool temporary = false);

    /**
     * @brief Imposta un colore RGB personalizzato (stato CUSTOM)
     * @param r Rosso (0-255)
     * @param g Verde (0-255)
     * @param b Blu (0-255)
     */
    void setColor(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Imposta pattern Pulse con colore personalizzato
     * @param r Rosso (0-255)
     * @param g Verde (0-255)
     * @param b Blu (0-255)
     */
    void setPulseColor(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Imposta pattern Strobe con colore personalizzato
     * @param r Rosso (0-255)
     * @param g Verde (0-255)
     * @param b Blu (0-255)
     */
    void setStrobeColor(uint8_t r, uint8_t g, uint8_t b);
    void setStrobePalette(const std::vector<uint32_t>& colors, size_t start_index = 0);
    void setPulsePalette(const std::vector<uint32_t>& colors, size_t start_index = 0);

    /**
     * @brief Imposta la velocità delle animazioni
     * @param speed Velocità 1-100 (default 50)
     */
    void setAnimationSpeed(uint8_t speed);

    /**
     * @brief Ottiene la velocità delle animazioni
     * @return Velocità 1-100
     */
    uint8_t getAnimationSpeed() const { return animation_speed_; }

    /**
     * @brief Spegne il LED
     */
    void off();

    /**
     * @brief Aggiorna l'animazione del LED (da chiamare periodicamente)
     */
    void update();

    /**
     * @brief Verifica se il LED è inizializzato
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Imposta il timeout di inattività prima di tornare allo stato idle
     * @param timeout_ms Timeout in millisecondi (0 = disabilitato)
     */
    void setIdleTimeout(uint32_t timeout_ms);

    /**
     * @brief Ottiene il timeout di inattività configurato
     * @return Timeout in millisecondi
     */
    uint32_t getIdleTimeout() const { return idle_timeout_ms_; }

    /**
     * @brief Imposta lo stato idle di default (dove tornare dopo il timeout)
     * @param state Stato idle
     */
    void setIdleState(LedState state);

    /**
     * @brief Ottiene lo stato attuale del LED
     */
    LedState getCurrentState() const { return current_state_; }

private:
    RgbLedManager() = default;
    RgbLedManager(const RgbLedManager&) = delete;
    RgbLedManager& operator=(const RgbLedManager&) = delete;

    void setPixel(uint8_t r, uint8_t g, uint8_t b);
    void updateAnimation();

    static void led_task(void* parameter);

    bool initialized_ = false;
    uint8_t pin_ = 42;
    uint8_t brightness_ = 50;  // Default 50%

    static constexpr rmt_channel_t kRmtChannel = RMT_CHANNEL_0;
    rmt_channel_t led_channel_ = kRmtChannel;

    SemaphoreHandle_t state_mutex_ = nullptr;  // Protegge accessi concorrenti allo stato
    LedState current_state_ = LedState::OFF;
    LedState idle_state_ = LedState::OFF;           // Stato a cui tornare dopo timeout
    LedState previous_state_ = LedState::OFF;        // Stato precedente (per temporary)
    bool is_temporary_ = false;                      // Se lo stato attuale è temporaneo

    uint8_t custom_r_ = 0;
    uint8_t custom_g_ = 0;
    uint8_t custom_b_ = 0;

    uint8_t pulse_r_ = 255;  // Default pulse color (purple)
    uint8_t pulse_g_ = 100;
    uint8_t pulse_b_ = 200;

    uint8_t strobe_r_ = 255; // Default strobe color (white)
    uint8_t strobe_g_ = 255;
    uint8_t strobe_b_ = 255;
    std::vector<std::array<uint8_t, 3>> strobe_palette_;
    size_t strobe_palette_index_ = 0;

    std::vector<std::array<uint8_t, 3>> pulse_palette_;
    size_t pulse_palette_index_ = 0;

    // Per animazioni
    uint32_t last_update_ = 0;
    uint32_t last_activity_ = 0;                     // Ultimo cambio stato
    uint32_t idle_timeout_ms_ = 30000;               // Default 30 secondi
    uint8_t animation_phase_ = 0;
    uint8_t animation_speed_ = 50;                   // Velocità animazioni (1-100)
    bool blink_on_ = false;
};

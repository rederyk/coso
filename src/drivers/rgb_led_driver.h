#pragma once

#include <Arduino.h>
#include <cstdint>
#include <driver/rmt.h>

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
        CUSTOM            // Colore personalizzato
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
     */
    void setState(LedState state);

    /**
     * @brief Imposta un colore RGB personalizzato
     * @param r Rosso (0-255)
     * @param g Verde (0-255)
     * @param b Blu (0-255)
     */
    void setColor(uint8_t r, uint8_t g, uint8_t b);

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

    LedState current_state_ = LedState::OFF;
    uint8_t custom_r_ = 0;
    uint8_t custom_g_ = 0;
    uint8_t custom_b_ = 0;

    // Per animazioni
    uint32_t last_update_ = 0;
    uint8_t animation_phase_ = 0;
    bool blink_on_ = false;
};

#include "rgb_led_driver.h"
#include "utils/logger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static TaskHandle_t led_task_handle = nullptr;

static constexpr uint16_t kWs2812T0H = 4;
static constexpr uint16_t kWs2812T0L = 9;
static constexpr uint16_t kWs2812T1H = 8;
static constexpr uint16_t kWs2812T1L = 5;

RgbLedManager& RgbLedManager::getInstance() {
    static RgbLedManager instance;
    return instance;
}

RgbLedManager::~RgbLedManager() {
    if (led_task_handle) {
        vTaskDelete(led_task_handle);
        led_task_handle = nullptr;
    }
    if (initialized_) {
        rmt_driver_uninstall(led_channel_);
    }
    initialized_ = false;
}

bool RgbLedManager::begin(uint8_t pin) {
    if (initialized_) {
        return true;
    }
    pin_ = pin;

    Logger::getInstance().info("[RGB LED] Initializing with new RMT driver...");
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX((gpio_num_t)pin_, led_channel_);
    config.clk_div = 8;  // 80MHz / 8 = 10MHz, matches our timing
    config.mem_block_num = 1;
    config.tx_config.carrier_en = false;
    config.tx_config.loop_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

    esp_err_t err = rmt_config(&config);
    if (err != ESP_OK) {
        Logger::getInstance().errorf("[RGB LED] Failed to configure RMT channel: %s", esp_err_to_name(err));
        return false;
    }

    err = rmt_driver_install(config.channel, 0, 0);
    if (err != ESP_OK) {
        Logger::getInstance().errorf("[RGB LED] Failed to install RMT driver: %s", esp_err_to_name(err));
        return false;
    }

    led_channel_ = config.channel;

    initialized_ = true;
    off();

    xTaskCreatePinnedToCore(
        led_task,
        "rgb_led_task",
        2048,
        this,
        0,
        &led_task_handle,
        1
    );

    Logger::getInstance().infof("[RGB LED] Initialized on GPIO%d", pin_);
    return true;
}

void RgbLedManager::setBrightness(uint8_t brightness) {
    brightness_ = constrain(brightness, 0, 100);
    if (current_state_ == LedState::CUSTOM) {
        setColor(custom_r_, custom_g_, custom_b_);
    } else {
        updateAnimation();
    }
}

void RgbLedManager::setState(LedState state, bool temporary) {
    // Log del cambio di stato
    const char* state_name = "UNKNOWN";
    switch (state) {
        case LedState::OFF: state_name = "OFF"; break;
        case LedState::WIFI_CONNECTING: state_name = "WIFI_CONNECTING"; break;
        case LedState::WIFI_CONNECTED: state_name = "WIFI_CONNECTED"; break;
        case LedState::WIFI_ERROR: state_name = "WIFI_ERROR"; break;
        case LedState::BLE_ADVERTISING: state_name = "BLE_ADVERTISING"; break;
        case LedState::BLE_CONNECTED: state_name = "BLE_CONNECTED"; break;
        case LedState::BOOT: state_name = "BOOT"; break;
        case LedState::ERROR: state_name = "ERROR"; break;
        case LedState::CUSTOM: state_name = "CUSTOM"; break;
        case LedState::RAINBOW: state_name = "RAINBOW"; break;
        case LedState::STROBE: state_name = "STROBE"; break;
        case LedState::PULSE: state_name = "PULSE"; break;
        case LedState::RGB_CYCLE: state_name = "RGB_CYCLE"; break;
        case LedState::PULSE_CUSTOM: state_name = "PULSE_CUSTOM"; break;
        case LedState::STROBE_CUSTOM: state_name = "STROBE_CUSTOM"; break;
    }

    Logger::getInstance().infof("[RGB LED] State change: %s -> %s %s",
        state_name,
        temporary ? "(temporary)" : "",
        is_temporary_ ? "[will revert after timeout]" : "");

    // Salva lo stato precedente se è temporaneo
    if (temporary && !is_temporary_) {
        previous_state_ = current_state_;
    }

    current_state_ = state;
    is_temporary_ = temporary;
    animation_phase_ = 0;
    blink_on_ = false;
    last_update_ = millis();
    last_activity_ = millis();  // Reset del timer di inattività
    updateAnimation();
}

void RgbLedManager::setColor(uint8_t r, uint8_t g, uint8_t b) {
    custom_r_ = r;
    custom_g_ = g;
    custom_b_ = b;
    current_state_ = LedState::CUSTOM;
    setPixel(r, g, b);
}

void RgbLedManager::setPulseColor(uint8_t r, uint8_t g, uint8_t b) {
    pulse_r_ = r;
    pulse_g_ = g;
    pulse_b_ = b;
    current_state_ = LedState::PULSE_CUSTOM;
    animation_phase_ = 0;
    last_update_ = millis();
    updateAnimation();
    Logger::getInstance().infof("[RGB LED] Pulse color set to RGB(%d, %d, %d)", r, g, b);
}

void RgbLedManager::setStrobeColor(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t hex_color = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
    setStrobePalette({hex_color}, 0);
    current_state_ = LedState::STROBE_CUSTOM;
    animation_phase_ = 0;
    blink_on_ = false;
    last_update_ = millis();
    updateAnimation();
    Logger::getInstance().infof("[RGB LED] Strobe color set to RGB(%d, %d, %d)", r, g, b);
}

void RgbLedManager::setStrobePalette(const std::vector<uint32_t>& colors, size_t start_index) {
    strobe_palette_.clear();
    strobe_palette_index_ = 0;

    for (uint32_t color : colors) {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        strobe_palette_.push_back({r, g, b});
    }

    if (strobe_palette_.empty()) {
        strobe_palette_.push_back({strobe_r_, strobe_g_, strobe_b_});
    }

    size_t palette_size = strobe_palette_.size();
    if (palette_size > 0) {
        if (start_index >= palette_size) {
            start_index %= palette_size;
        }
        strobe_palette_index_ = start_index;
        const auto& entry = strobe_palette_[strobe_palette_index_];
        strobe_r_ = entry[0];
        strobe_g_ = entry[1];
        strobe_b_ = entry[2];
    }

    Logger::getInstance().infof("[RGB LED] Strobe palette configured (%u colors)", (unsigned)strobe_palette_.size());
}

void RgbLedManager::off() {
    current_state_ = LedState::OFF;
    setPixel(0, 0, 0);
}

void RgbLedManager::update() {
    if (!initialized_) return;

    uint32_t now = millis();
    uint32_t elapsed = now - last_update_;

    // Controlla timeout di inattività per stati temporanei
    if (is_temporary_ && idle_timeout_ms_ > 0) {
        uint32_t idle_time = now - last_activity_;
        if (idle_time >= idle_timeout_ms_) {
            Logger::getInstance().infof("[RGB LED] Idle timeout reached, reverting to previous state");
            LedState revert_to = previous_state_;
            is_temporary_ = false;
            setState(revert_to, false);
            return;
        }
    }

    // Calcola intervallo basato sulla velocità (più velocità = meno tempo)
    uint32_t base_interval = 100;
    uint32_t speed_factor = (101 - animation_speed_); // Inverte: speed 100 = factor 1, speed 1 = factor 100
    uint32_t interval = (base_interval * speed_factor) / 50; // Normalizza

    switch (current_state_) {
        case LedState::WIFI_CONNECTING:
        case LedState::WIFI_ERROR:
        case LedState::BLE_ADVERTISING:
            if (elapsed >= 500) {
                blink_on_ = !blink_on_;
                last_update_ = now;
                // Log del lampeggio
                if (blink_on_) {
                    Logger::getInstance().debugf("[RGB LED] Blink ON");
                }
                updateAnimation();
            }
            break;
        case LedState::BOOT:
        case LedState::RAINBOW:
            if (elapsed >= interval) {
                animation_phase_ = (animation_phase_ + 5) % 256;
                last_update_ = now;
                updateAnimation();
            }
            break;
        case LedState::STROBE:
        case LedState::STROBE_CUSTOM:
            if (elapsed >= (interval / 4)) { // Strobe più veloce
                blink_on_ = !blink_on_;
                last_update_ = now;
                updateAnimation();
            }
            break;
        case LedState::PULSE:
        case LedState::PULSE_CUSTOM:
            if (elapsed >= interval) {
                animation_phase_ = (animation_phase_ + 2) % 256;
                last_update_ = now;
                updateAnimation();
            }
            break;
        case LedState::RGB_CYCLE:
            if (elapsed >= (interval * 2)) { // RGB Cycle più lento
                animation_phase_ = (animation_phase_ + 1) % 3;
                last_update_ = now;
                updateAnimation();
            }
            break;
        default:
            break;
    }
}

void RgbLedManager::setPixel(uint8_t r, uint8_t g, uint8_t b) {
    if (!initialized_) {
        return;
    }

    r = (r * brightness_) / 100;
    g = (g * brightness_) / 100;
    b = (b * brightness_) / 100;

    const uint8_t grb[3] = {g, r, b};
    rmt_item32_t items[24] = {};
    size_t index = 0;

    for (uint8_t color : grb) {
        for (int bit = 7; bit >= 0; --bit) {
            bool high = color & (1 << bit);
            items[index].duration0 = high ? kWs2812T1H : kWs2812T0H;
            items[index].level0 = 1;
            items[index].duration1 = high ? kWs2812T1L : kWs2812T0L;
            items[index].level1 = 0;
            ++index;
        }
    }

    esp_err_t err = rmt_write_items(led_channel_, items, index, false);
    if (err == ESP_OK) {
        err = rmt_wait_tx_done(led_channel_, portMAX_DELAY);
    }
    if (err != ESP_OK) {
        static bool transmit_error_logged = false;
        if (!transmit_error_logged) {
            Logger::getInstance().errorf("[RGB LED] RMT transmit failed: %s", esp_err_to_name(err));
            transmit_error_logged = true;
        }
    }
}

void RgbLedManager::updateAnimation() {
    uint8_t r = 0, g = 0, b = 0;

    switch (current_state_) {
        case LedState::OFF:
            r = g = b = 0;
            break;
        case LedState::WIFI_CONNECTING:
            if (blink_on_) { r = 0; g = 0; b = 255; } // Blu
            break;
        case LedState::WIFI_CONNECTED:
            r = 0; g = 255; b = 0; // Verde
            break;
        case LedState::WIFI_ERROR:
            if (blink_on_) { r = 255; g = 0; b = 0; } // Rosso
            break;
        case LedState::BLE_ADVERTISING:
            if (blink_on_) { r = 0; g = 255; b = 255; } // Ciano
            break;
        case LedState::BLE_CONNECTED:
            r = 0; g = 255; b = 255; // Ciano
            break;
        case LedState::BOOT:
        case LedState::RAINBOW:
            // Arcobaleno fluido
            if (animation_phase_ < 85) {
                r = animation_phase_ * 3;
                g = 255 - animation_phase_ * 3;
                b = 0;
            } else if (animation_phase_ < 170) {
                r = 255 - (animation_phase_ - 85) * 3;
                g = 0;
                b = (animation_phase_ - 85) * 3;
            } else {
                r = 0;
                g = (animation_phase_ - 170) * 3;
                b = 255 - (animation_phase_ - 170) * 3;
            }
            break;
        case LedState::STROBE:
            // Lampeggio bianco veloce
            if (blink_on_) {
                r = g = b = 255;
            }
            break;
        case LedState::STROBE_CUSTOM:
            if (blink_on_) {
                if (!strobe_palette_.empty()) {
                    const auto& entry = strobe_palette_[strobe_palette_index_];
                    r = entry[0];
                    g = entry[1];
                    b = entry[2];
                    strobe_palette_index_ = (strobe_palette_index_ + 1) % strobe_palette_.size();
                } else {
                    r = strobe_r_;
                    g = strobe_g_;
                    b = strobe_b_;
                }
            }
            break;
        case LedState::PULSE:
            // Respiro sinusoidale (colore fisso violet-blue)
            {
                float brightness_factor = (sin(animation_phase_ * 2.0f * PI / 256.0f) + 1.0f) / 2.0f;
                r = (uint8_t)(255 * brightness_factor);
                g = (uint8_t)(100 * brightness_factor);
                b = (uint8_t)(200 * brightness_factor);
            }
            break;
        case LedState::PULSE_CUSTOM:
            // Pulse con colore personalizzato
            {
                float brightness_factor = (sin(animation_phase_ * 2.0f * PI / 256.0f) + 1.0f) / 2.0f;
                r = (uint8_t)(pulse_r_ * brightness_factor);
                g = (uint8_t)(pulse_g_ * brightness_factor);
                b = (uint8_t)(pulse_b_ * brightness_factor);
            }
            break;
        case LedState::RGB_CYCLE:
            // Ciclo RGB: Rosso -> Verde -> Blu
            if (animation_phase_ == 0) {
                r = 255; g = 0; b = 0;
            } else if (animation_phase_ == 1) {
                r = 0; g = 255; b = 0;
            } else {
                r = 0; g = 0; b = 255;
            }
            break;
        case LedState::ERROR:
            r = 255; g = 0; b = 0; // Rosso fisso
            break;
        case LedState::CUSTOM:
            r = custom_r_;
            g = custom_g_;
            b = custom_b_;
            break;
    }
    setPixel(r, g, b);
}

void RgbLedManager::setAnimationSpeed(uint8_t speed) {
    animation_speed_ = constrain(speed, 1, 100);
    Logger::getInstance().infof("[RGB LED] Animation speed set to %u", animation_speed_);
}

void RgbLedManager::setIdleTimeout(uint32_t timeout_ms) {
    idle_timeout_ms_ = timeout_ms;
    Logger::getInstance().infof("[RGB LED] Idle timeout set to %u ms", timeout_ms);
}

void RgbLedManager::setIdleState(LedState state) {
    idle_state_ = state;
    Logger::getInstance().infof("[RGB LED] Idle state set");
}

void RgbLedManager::led_task(void* parameter) {
    RgbLedManager* manager = static_cast<RgbLedManager*>(parameter);
    while (true) {
        manager->update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

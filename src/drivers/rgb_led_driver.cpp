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

void RgbLedManager::setState(LedState state) {
    current_state_ = state;
    animation_phase_ = 0;
    blink_on_ = false;
    last_update_ = millis();
    updateAnimation();
}

void RgbLedManager::setColor(uint8_t r, uint8_t g, uint8_t b) {
    custom_r_ = r;
    custom_g_ = g;
    custom_b_ = b;
    current_state_ = LedState::CUSTOM;
    setPixel(r, g, b);
}

void RgbLedManager::off() {
    current_state_ = LedState::OFF;
    setPixel(0, 0, 0);
}

void RgbLedManager::update() {
    if (!initialized_) return;

    uint32_t now = millis();
    uint32_t elapsed = now - last_update_;

    switch (current_state_) {
        case LedState::WIFI_CONNECTING:
        case LedState::WIFI_ERROR:
        case LedState::BLE_ADVERTISING:
            if (elapsed >= 500) {
                blink_on_ = !blink_on_;
                last_update_ = now;
                updateAnimation();
            }
            break;
        case LedState::BOOT:
            if (elapsed >= 100) {
                animation_phase_ = (animation_phase_ + 10) % 256;
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

void RgbLedManager::led_task(void* parameter) {
    RgbLedManager* manager = static_cast<RgbLedManager*>(parameter);
    while (true) {
        manager->update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

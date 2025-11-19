#include "core/backlight_manager.h"
#include <Arduino.h>
#include "utils/logger.h"

#ifdef TFT_BL
static constexpr int BACKLIGHT_PIN = TFT_BL;
#else
static constexpr int BACKLIGHT_PIN = -1;
#endif

BacklightManager& BacklightManager::getInstance() {
    static BacklightManager instance;
    return instance;
}

void BacklightManager::begin() {
    if (initialized_) {
        return;
    }

    if (BACKLIGHT_PIN < 0) {
        Logger::getInstance().warn("[Backlight] No backlight pin defined");
        return;
    }

    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(BACKLIGHT_PIN, PWM_CHANNEL);

    initialized_ = true;
    applyPWM(current_brightness_);
    Logger::getInstance().infof("[Backlight] Initialized on pin %d at %u%%", BACKLIGHT_PIN, current_brightness_);
}

void BacklightManager::setBrightness(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    } else if (percent == 0) {
        percent = 1;
    }
    current_brightness_ = percent;
    applyPWM(percent);
}

void BacklightManager::applyPWM(uint8_t percent) {
    if (!initialized_ || BACKLIGHT_PIN < 0) {
        return;
    }

    const uint32_t duty = (percent * 255) / 100;
    ledcWrite(PWM_CHANNEL, duty);
}

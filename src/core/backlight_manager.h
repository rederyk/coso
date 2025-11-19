#pragma once

#include <cstdint>

class BacklightManager {
public:
    static BacklightManager& getInstance();

    void begin();
    void setBrightness(uint8_t percent);
    uint8_t getBrightness() const { return current_brightness_; }

private:
    BacklightManager() = default;

    void applyPWM(uint8_t percent);

    bool initialized_ = false;
    uint8_t current_brightness_ = 100;

    static constexpr uint8_t PWM_CHANNEL = 0;
    static constexpr uint32_t PWM_FREQUENCY = 5000;
    static constexpr uint8_t PWM_RESOLUTION = 8;
};

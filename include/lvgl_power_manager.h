/**
 * @file lvgl_power_manager.h
 * @brief LVGL Power Management - Suspend/Resume for DRAM optimization
 *
 * Manages LVGL lifecycle to free DRAM when UI is not needed.
 * Frees ~80-100KB when suspended.
 */

#pragma once

#include <Arduino.h>
#include <string>

enum class SystemMode {
    MODE_UI,           // LVGL active, voice assistant suspended
    MODE_VOICE,        // LVGL suspended, voice assistant active
    MODE_HYBRID        // Both active (if enough RAM available)
};

enum class LVGLState {
    ACTIVE,            // Fully running
    SUSPENDED,         // Paused, memory reduced
    INACTIVE           // Completely deinitialized
};

class LVGLPowerManager {
public:
    static LVGLPowerManager& getInstance();

    // Lifecycle management
    void init();
    bool suspend();      // Pause LVGL, free ~80-100KB
    bool resume();       // Resume LVGL
    bool deinit();       // Full shutdown, free ~125KB
    bool reinit();       // Full reinit (slower)

    // Mode switching
    void switchToUIMode();
    void switchToVoiceMode();
    void switchToHybridMode();

    // State queries
    SystemMode getCurrentMode() const { return current_mode; }
    LVGLState getLVGLState() const { return lvgl_state; }
    bool isLVGLActive() const { return lvgl_state == LVGLState::ACTIVE; }
    bool isLVGLSuspended() const { return lvgl_state == LVGLState::SUSPENDED; }

    // Auto-suspend settings
    void setAutoSuspendEnabled(bool enabled) { auto_suspend_enabled = enabled; }
    void setAutoSuspendTimeout(uint32_t timeout_ms) { auto_suspend_timeout = timeout_ms; }
    void resetIdleTimer();

    // Memory stats
    uint32_t getFreeDRAM() const;
    uint32_t getFreePSRAM() const;
    void printMemoryStats() const;
    void printDetailedDRAMUsage() const;  // Analizza cosa occupa DRAM

    // Called from main loop
    void update();

    // Event callbacks
    void onTouchDetected();
    void onWakeWordDetected();
    void onScreenTimeout();

private:
    LVGLPowerManager() = default;
    ~LVGLPowerManager() = default;
    LVGLPowerManager(const LVGLPowerManager&) = delete;
    LVGLPowerManager& operator=(const LVGLPowerManager&) = delete;

    // Internal state
    SystemMode current_mode = SystemMode::MODE_UI;
    LVGLState lvgl_state = LVGLState::ACTIVE;

    // Auto-suspend
    bool auto_suspend_enabled = true;
    uint32_t auto_suspend_timeout = 30000;  // 30 seconds default
    uint32_t last_activity_time = 0;

    // Memory tracking
    uint32_t dram_before_suspend = 0;
    uint32_t dram_after_suspend = 0;
    uint8_t previous_brightness = 100;
    std::string last_active_app_id;
    bool ui_resources_released = false;

    // Helper functions
    void pauseLVGLTimers();
    void resumeLVGLTimers();
    void cleanLVGLCaches();
    bool startVoiceAssistant();
    bool stopVoiceAssistant();
};

// Global access helper
#define LVGLPowerMgr LVGLPowerManager::getInstance()

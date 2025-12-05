/**
 * @file lvgl_power_manager.cpp
 * @brief Implementation of LVGL Power Management
 */

#include "lvgl_power_manager.h"
#include "lvgl.h"
#include "core/app_manager.h"
#include "drivers/touch_driver.h"
#include "backlight_manager.h"
#include "voice_assistant.h"
#include "utils/lvgl_mutex.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char* TAG = "LVGLPowerMgr";

LVGLPowerManager& LVGLPowerManager::getInstance() {
    static LVGLPowerManager instance;
    return instance;
}

void LVGLPowerManager::init() {
    ESP_LOGI(TAG, "Initializing LVGL Power Manager");

    // Set initial state
    lvgl_state = LVGLState::ACTIVE;
    current_mode = SystemMode::MODE_UI;
    last_activity_time = millis();

    // Print memory stats (safe, doesn't touch LVGL)
    ESP_LOGI(TAG, "LVGL Power Manager initialized");
    printMemoryStats();
}

bool LVGLPowerManager::suspend() {
    if (lvgl_state == LVGLState::SUSPENDED) {
        ESP_LOGW(TAG, "LVGL already suspended");
        return true;
    }

    if (lvgl_state != LVGLState::ACTIVE) {
        ESP_LOGE(TAG, "Cannot suspend - LVGL not active");
        return false;
    }

    ESP_LOGI(TAG, "Suspending LVGL...");
    dram_before_suspend = getFreeDRAM();

    // CRITICAL: Lock LVGL mutex before modifying LVGL state
    const bool already_owned = lvgl_mutex_is_owned_by_current_task();
    bool lock_acquired = false;
    if (!already_owned) {
        if (!lvgl_mutex_lock(pdMS_TO_TICKS(5000))) {
            ESP_LOGE(TAG, "Failed to acquire LVGL lock for suspend");
            return false;
        }
        lock_acquired = true;
    } else {
        ESP_LOGD(TAG, "LVGL mutex already owned by current task - skipping lock");
    }

    // Pause all LVGL timers
    pauseLVGLTimers();

    // Clean caches and temporary buffers
    cleanLVGLCaches();

    // Destroy LVGL UI resources to free heap
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager && app_manager->isInitialized()) {
        last_active_app_id = app_manager->getCurrentAppId();
        if (last_active_app_id.empty()) {
            last_active_app_id = "dashboard";
        }
        app_manager->destroyUI();
        ui_resources_released = true;
    }

    lvgl_state = LVGLState::SUSPENDED;

    if (lock_acquired) {
        lvgl_mutex_unlock();
    }

    // Backlight off (saves power too) - do this AFTER unlocking
    BacklightManager& backlight = BacklightManager::getInstance();
    previous_brightness = backlight.getBrightness();
    backlight.setBrightness(0);

    dram_after_suspend = getFreeDRAM();

    uint32_t freed = dram_after_suspend - dram_before_suspend;
    ESP_LOGI(TAG, "LVGL suspended. Freed ~%lu KB DRAM", freed / 1024);
    printMemoryStats();

    return true;
}

bool LVGLPowerManager::resume() {
    if (lvgl_state == LVGLState::ACTIVE) {
        ESP_LOGW(TAG, "LVGL already active");
        return true;
    }

    if (lvgl_state != LVGLState::SUSPENDED) {
        ESP_LOGE(TAG, "Cannot resume - LVGL not suspended");
        return false;
    }

    ESP_LOGI(TAG, "Resuming LVGL...");

    // CRITICAL: Lock LVGL mutex before modifying LVGL state
    const bool already_owned = lvgl_mutex_is_owned_by_current_task();
    bool lock_acquired = false;
    if (!already_owned) {
        if (!lvgl_mutex_lock(pdMS_TO_TICKS(5000))) {
            ESP_LOGE(TAG, "Failed to acquire LVGL lock for resume");
            return false;
        }
        lock_acquired = true;
    } else {
        ESP_LOGD(TAG, "LVGL mutex already owned by current task - skipping lock");
    }

    // Resume timers
    resumeLVGLTimers();

    // Rebuild UI resources if they were released during suspend
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager && app_manager->isInitialized() && ui_resources_released) {
        app_manager->restoreUI(last_active_app_id);
        ui_resources_released = false;
    }

    // Force full refresh
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

    lvgl_state = LVGLState::ACTIVE;
    last_activity_time = millis();

    if (lock_acquired) {
        lvgl_mutex_unlock();
    }

    // Restore backlight to previous level
    BacklightManager::getInstance().setBrightness(previous_brightness);

    ESP_LOGI(TAG, "LVGL resumed");
    printMemoryStats();

    return true;
}

bool LVGLPowerManager::deinit() {
    if (lvgl_state == LVGLState::INACTIVE) {
        ESP_LOGW(TAG, "LVGL already deinitialized");
        return true;
    }

    ESP_LOGI(TAG, "Deinitializing LVGL...");
    ESP_LOGW(TAG, "Full deinit not fully supported in LVGL 8.4 - using deep suspend instead");
    dram_before_suspend = getFreeDRAM();

    // Backlight off
    BacklightManager::getInstance().setBrightness(0);

    // Note: lv_deinit() doesn't exist in LVGL 8.4, so we do deep suspend
    // This includes pausing timers + cleaning caches
    pauseLVGLTimers();
    cleanLVGLCaches();

    lvgl_state = LVGLState::INACTIVE;
    dram_after_suspend = getFreeDRAM();

    uint32_t freed = dram_after_suspend - dram_before_suspend;
    ESP_LOGI(TAG, "LVGL deinitialized (deep suspend). Freed ~%lu KB DRAM", freed / 1024);
    printMemoryStats();

    return true;
}

bool LVGLPowerManager::reinit() {
    if (lvgl_state != LVGLState::INACTIVE) {
        ESP_LOGE(TAG, "Cannot reinit - LVGL not inactive");
        return false;
    }

    ESP_LOGI(TAG, "Reinitializing LVGL...");

    // Note: Full reinit requires display_manager to be called
    // This is a placeholder - actual implementation depends on your setup
    ESP_LOGE(TAG, "Full reinit not implemented - use suspend/resume instead");

    return false;
}

void LVGLPowerManager::switchToUIMode() {
    if (current_mode == SystemMode::MODE_UI) return;

    ESP_LOGI(TAG, "Switching to UI Mode");

    // Stop voice assistant if running
    stopVoiceAssistant();

    // Resume LVGL
    resume();

    current_mode = SystemMode::MODE_UI;
}

void LVGLPowerManager::switchToVoiceMode() {
    if (current_mode == SystemMode::MODE_VOICE) return;

    ESP_LOGI(TAG, "Switching to Voice Mode");

    // Suspend LVGL to free RAM
    suspend();

    // Start voice assistant
    startVoiceAssistant();

    current_mode = SystemMode::MODE_VOICE;
}

void LVGLPowerManager::switchToHybridMode() {
    ESP_LOGI(TAG, "Switching to Hybrid Mode");

    // Check if we have enough RAM
    uint32_t free_dram = getFreeDRAM();
    if (free_dram < 50000) {  // Need at least 50KB free
        ESP_LOGW(TAG, "Not enough DRAM for hybrid mode (%lu KB free)", free_dram / 1024);
        ESP_LOGI(TAG, "Staying in current mode");
        return;
    }

    // Resume LVGL if suspended
    if (lvgl_state == LVGLState::SUSPENDED) {
        resume();
    }

    // Keep voice assistant running
    if (current_mode != SystemMode::MODE_VOICE) {
        startVoiceAssistant();
    }

    current_mode = SystemMode::MODE_HYBRID;
}

void LVGLPowerManager::resetIdleTimer() {
    last_activity_time = millis();
}

void LVGLPowerManager::update() {
    // Check auto-suspend timeout
    if (auto_suspend_enabled &&
        current_mode == SystemMode::MODE_UI &&
        lvgl_state == LVGLState::ACTIVE) {

        uint32_t idle_time = millis() - last_activity_time;
        if (idle_time >= auto_suspend_timeout) {
            ESP_LOGI(TAG, "Auto-suspend triggered after %lu ms idle", idle_time);
            switchToVoiceMode();
        }
    }

    // When LVGL is suspended, poll touch controller to detect wake gestures
    if (lvgl_state == LVGLState::SUSPENDED &&
        touch_driver_available() &&
        touch_driver_has_touch()) {
        ESP_LOGI(TAG, "Touch detected while LVGL suspended - resuming UI");
        onTouchDetected();
    }
}

void LVGLPowerManager::onTouchDetected() {
    ESP_LOGD(TAG, "Touch detected");
    resetIdleTimer();

    // If in voice mode, switch back to UI
    if (current_mode == SystemMode::MODE_VOICE) {
        switchToUIMode();
    }
}

void LVGLPowerManager::onWakeWordDetected() {
    ESP_LOGI(TAG, "Wake word detected");

    // Voice assistant can work even with screen off
    if (current_mode == SystemMode::MODE_UI) {
        // Optionally switch to hybrid mode for visual feedback
        // switchToHybridMode();
    }
}

void LVGLPowerManager::onScreenTimeout() {
    ESP_LOGI(TAG, "Screen timeout");
    switchToVoiceMode();
}

// Private helper functions

void LVGLPowerManager::pauseLVGLTimers() {
    ESP_LOGD(TAG, "Pausing LVGL timers");

    // Pause all timers
    lv_timer_t* timer = lv_timer_get_next(NULL);
    while (timer) {
        lv_timer_pause(timer);
        timer = lv_timer_get_next(timer);
    }
}

void LVGLPowerManager::resumeLVGLTimers() {
    ESP_LOGD(TAG, "Resuming LVGL timers");

    // Resume all timers
    lv_timer_t* timer = lv_timer_get_next(NULL);
    while (timer) {
        lv_timer_resume(timer);
        timer = lv_timer_get_next(timer);
    }
}

void LVGLPowerManager::cleanLVGLCaches() {
    ESP_LOGD(TAG, "Cleaning LVGL caches");

    // Get default display
    lv_disp_t* disp = lv_disp_get_default();
    if (disp) {
        // Clean display cache
        lv_disp_clean_dcache(disp);
    }

    // Release temporary buffers and cached images
    lv_mem_buf_free_all();
    lv_img_cache_invalidate_src(NULL);

    // Note: lv_mem_monitor doesn't work with custom PSRAM allocator
    // Memory cleanup is handled automatically by LVGL
    ESP_LOGD(TAG, "Cache cleanup complete");
}

bool LVGLPowerManager::startVoiceAssistant() {
    ESP_LOGI(TAG, "Starting voice assistant");

    // Check if voice assistant manager exists
    extern VoiceAssistant* g_voice_assistant;
    if (g_voice_assistant) {
        // Voice assistant already initialized in main.cpp via begin()
        // Just log that it's available for use
        ESP_LOGI(TAG, "Voice assistant ready for use");
        return true;
    }

    ESP_LOGW(TAG, "Voice assistant not available");
    return false;
}

bool LVGLPowerManager::stopVoiceAssistant() {
    ESP_LOGI(TAG, "Stopping voice assistant");

    extern VoiceAssistant* g_voice_assistant;
    if (g_voice_assistant) {
        // Voice assistant cleanup managed by its own lifecycle
        ESP_LOGI(TAG, "Voice assistant remains available");
        return true;
    }

    return false;
}

// Memory utilities

uint32_t LVGLPowerManager::getFreeDRAM() const {
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

uint32_t LVGLPowerManager::getFreePSRAM() const {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

void LVGLPowerManager::printMemoryStats() const {
    uint32_t free_dram = getFreeDRAM();
    uint32_t total_dram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t free_psram = getFreePSRAM();
    uint32_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "=== Memory Stats ===");
    ESP_LOGI(TAG, "DRAM:  %6lu / %6lu KB free (%.1f%%)",
             free_dram / 1024, total_dram / 1024,
             (float)free_dram / total_dram * 100.0f);
    ESP_LOGI(TAG, "PSRAM: %6lu / %6lu KB free (%.1f%%)",
             free_psram / 1024, total_psram / 1024,
             (float)free_psram / total_psram * 100.0f);
    ESP_LOGI(TAG, "Mode: %s, LVGL State: %s",
             current_mode == SystemMode::MODE_UI ? "UI" :
             current_mode == SystemMode::MODE_VOICE ? "VOICE" : "HYBRID",
             lvgl_state == LVGLState::ACTIVE ? "ACTIVE" :
             lvgl_state == LVGLState::SUSPENDED ? "SUSPENDED" : "INACTIVE");
    ESP_LOGI(TAG, "==================");
}

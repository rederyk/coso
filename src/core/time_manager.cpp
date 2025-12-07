#include "core/time_manager.h"
#include "utils/logger.h"

#include <esp_sntp.h>
#include <sys/time.h>
#include <lwip/apps/sntp.h>

static const char* TAG = "TimeManager";

// SNTP sync notification callback
static void time_sync_notification_cb(struct timeval* tv) {
    Logger::getInstance().info("Time synchronized via NTP");
}

TimeManager& TimeManager::getInstance() {
    static TimeManager instance;
    return instance;
}

TimeManager::TimeManager() {
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        Logger::getInstance().error("TimeManager: Failed to create mutex");
    }
}

TimeManager::~TimeManager() {
    if (sync_task_handle_) {
        vTaskDelete(sync_task_handle_);
    }
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

bool TimeManager::begin() {
    if (initialized_) {
        Logger::getInstance().warn("TimeManager: Already initialized");
        return true;
    }

    Logger::getInstance().info("TimeManager: Initializing");

    // Set timezone
    setenv("TZ", timezone_.c_str(), 1);
    tzset();
    Logger::getInstance().infof("TimeManager: Timezone set to: %s", timezone_.c_str());

    // Configure SNTP
    esp_sntp_setoperatingmode((esp_sntp_operatingmode_t)SNTP_OPMODE_POLL);

    // Set NTP servers
    esp_sntp_setservername(0, ntp_server_primary_.c_str());
    if (!ntp_server_secondary_.empty()) {
        esp_sntp_setservername(1, ntp_server_secondary_.c_str());
    }
    if (!ntp_server_tertiary_.empty()) {
        esp_sntp_setservername(2, ntp_server_tertiary_.c_str());
    }

    // Set time sync notification callback
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // Enable smooth time adjustment (instead of step)
    // This prevents sudden time jumps
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);

    // Start SNTP service
    esp_sntp_init();
    Logger::getInstance().info("TimeManager: SNTP service started");
    Logger::getInstance().infof("TimeManager: NTP servers: %s, %s, %s",
                 ntp_server_primary_.c_str(),
                 ntp_server_secondary_.c_str(),
                 ntp_server_tertiary_.c_str());

    initialized_ = true;

    // Wait for initial sync (with timeout)
    if (waitForSync(10000)) {
        Logger::getInstance().info("TimeManager: Initial time sync successful");
        synchronized_ = true;
        last_sync_time_ = ::time(nullptr);
        sync_count_++;
    } else {
        Logger::getInstance().warn("TimeManager: Initial time sync timed out, will retry");
        sync_failures_++;
    }

    // Create auto-sync task
    if (auto_sync_enabled_) {
        xTaskCreatePinnedToCore(
            syncTask,
            "time_sync",
            4096,
            this,
            1,  // Low priority
            &sync_task_handle_,
            0   // Core 0
        );
        Logger::getInstance().infof("TimeManager: Auto-sync task created (interval: %lu hours)", auto_sync_interval_hours_);
    }

    return true;
}

bool TimeManager::syncNow(uint32_t timeout_ms) {
    if (!initialized_) {
        Logger::getInstance().error("TimeManager: Not initialized");
        return false;
    }

    Logger::getInstance().info("TimeManager: Manual sync requested");

    // Request SNTP sync
    esp_sntp_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_sntp_init();

    // Wait for sync
    if (waitForSync(timeout_ms)) {
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100))) {
            synchronized_ = true;
            last_sync_time_ = ::time(nullptr);
            sync_count_++;
            xSemaphoreGive(mutex_);
        }
        Logger::getInstance().info("TimeManager: Manual sync successful");
        return true;
    } else {
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100))) {
            sync_failures_++;
            xSemaphoreGive(mutex_);
        }
        Logger::getInstance().error("TimeManager: Manual sync failed");
        return false;
    }
}

bool TimeManager::waitForSync(uint32_t timeout_ms) {
    const uint32_t start = millis();
    const uint32_t check_interval = 100;

    while (millis() - start < timeout_ms) {
        time_t now = ::time(nullptr);

        // Check if we have valid time (year > 2020)
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year + 1900 > 2020) {
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(check_interval));
    }

    return false;
}

void TimeManager::syncTask(void* param) {
    TimeManager* tm = static_cast<TimeManager*>(param);

    Logger::getInstance().info("TimeManager: Auto-sync task started");

    while (true) {
        // Wait for sync interval
        const uint32_t interval_ms = tm->auto_sync_interval_hours_ * 3600 * 1000;
        vTaskDelay(pdMS_TO_TICKS(interval_ms));

        // Perform sync
        Logger::getInstance().info("TimeManager: Auto-sync triggered");
        tm->syncNow(10000);
    }
}

bool TimeManager::isSynchronized() const {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100))) {
        bool synced = synchronized_;
        xSemaphoreGive(mutex_);
        return synced;
    }
    return false;
}

time_t TimeManager::getLastSyncTime() const {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100))) {
        time_t t = last_sync_time_;
        xSemaphoreGive(mutex_);
        return t;
    }
    return 0;
}

time_t TimeManager::now() const {
    if (!synchronized_) {
        return 0;
    }
    return ::time(nullptr);
}

uint64_t TimeManager::nowMs() const {
    if (!synchronized_) {
        return 0;
    }

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

std::string TimeManager::getTimeString(const char* format) const {
    if (!synchronized_) {
        return "Not synchronized";
    }

    time_t now_time = now();
    struct tm timeinfo;
    localtime_r(&now_time, &timeinfo);

    char buffer[64];
    strftime(buffer, sizeof(buffer), format, &timeinfo);
    return std::string(buffer);
}

struct tm TimeManager::getLocalTime(time_t timestamp) const {
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    return timeinfo;
}

struct tm TimeManager::getLocalTime() const {
    time_t now_time = now();
    return getLocalTime(now_time);
}

void TimeManager::setTimezone(const std::string& tz) {
    Logger::getInstance().infof("TimeManager: Setting timezone: %s", tz.c_str());

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100))) {
        timezone_ = tz;
        setenv("TZ", timezone_.c_str(), 1);
        tzset();
        xSemaphoreGive(mutex_);

        Logger::getInstance().info("TimeManager: Timezone updated");
    }
}

void TimeManager::setNtpServers(const std::string& primary,
                                const std::string& secondary,
                                const std::string& tertiary) {
    Logger::getInstance().infof("TimeManager: Setting NTP servers: %s, %s, %s",
                 primary.c_str(), secondary.c_str(), tertiary.c_str());

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100))) {
        ntp_server_primary_ = primary;
        ntp_server_secondary_ = secondary;
        ntp_server_tertiary_ = tertiary;
        xSemaphoreGive(mutex_);

        // Restart SNTP with new servers
        if (initialized_) {
            esp_sntp_stop();
            vTaskDelay(pdMS_TO_TICKS(100));

            esp_sntp_setservername(0, ntp_server_primary_.c_str());
            if (!ntp_server_secondary_.empty()) {
                esp_sntp_setservername(1, ntp_server_secondary_.c_str());
            }
            if (!ntp_server_tertiary_.empty()) {
                esp_sntp_setservername(2, ntp_server_tertiary_.c_str());
            }

            esp_sntp_init();
            Logger::getInstance().info("TimeManager: SNTP restarted with new servers");
        }
    }
}

void TimeManager::setAutoSync(bool enabled, uint32_t interval_hours) {
    Logger::getInstance().infof("TimeManager: Auto-sync: %s, interval: %lu hours",
                 enabled ? "enabled" : "disabled", interval_hours);

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100))) {
        auto_sync_enabled_ = enabled;
        auto_sync_interval_hours_ = interval_hours;
        xSemaphoreGive(mutex_);
    }

    // Note: Task restart would be needed to apply new interval
    // For simplicity, this takes effect after task naturally wakes up
}

TimeManager::SyncStatus TimeManager::getSyncStatus() const {
    SyncStatus status;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100))) {
        status.synchronized = synchronized_;
        status.last_sync = last_sync_time_;
        status.sync_count = sync_count_;
        status.sync_failures = sync_failures_;
        status.ntp_server = ntp_server_primary_;
        xSemaphoreGive(mutex_);
    }

    return status;
}

#pragma once

#include <ctime>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * TimeManager - Manages system time synchronization via NTP
 *
 * Features:
 * - NTP synchronization with configurable servers
 * - Timezone support
 * - Auto-sync on WiFi connect
 * - Periodic re-sync to prevent drift
 * - Thread-safe time operations
 */
class TimeManager {
public:
    static TimeManager& getInstance();

    /**
     * Initialize time manager
     * Must be called after WiFi is connected
     */
    bool begin();

    /**
     * Manually trigger NTP sync
     * @param timeout_ms Maximum time to wait for sync (default 10s)
     * @return true if sync successful
     */
    bool syncNow(uint32_t timeout_ms = 10000);

    /**
     * Check if TimeManager is initialized
     * @return true if begin() was called successfully
     */
    bool isInitialized() const { return initialized_; }

    /**
     * Check if time is synchronized
     * @return true if we have valid time from NTP
     */
    bool isSynchronized() const;

    /**
     * Get last sync time (Unix timestamp)
     * @return Timestamp of last successful sync, 0 if never synced
     */
    time_t getLastSyncTime() const;

    /**
     * Get current Unix timestamp (seconds since epoch)
     * @return Current time, or 0 if not synchronized
     */
    time_t now() const;

    /**
     * Get current time in milliseconds since epoch
     * @return Current time in ms, or 0 if not synchronized
     */
    uint64_t nowMs() const;

    /**
     * Format current time as string
     * @param format strftime format string (default: "%Y-%m-%d %H:%M:%S")
     * @return Formatted time string
     */
    std::string getTimeString(const char* format = "%Y-%m-%d %H:%M:%S") const;

    /**
     * Get time_t from Unix timestamp
     * @param timestamp Unix timestamp (seconds)
     * @return time_t structure
     */
    struct tm getLocalTime(time_t timestamp) const;

    /**
     * Get current local time
     * @return tm structure with local time
     */
    struct tm getLocalTime() const;

    /**
     * Set timezone
     * @param tz Timezone string (e.g., "CET-1CEST,M3.5.0,M10.5.0/3" for Europe/Rome)
     */
    void setTimezone(const std::string& tz);

    /**
     * Get current timezone string
     */
    const std::string& getTimezone() const { return timezone_; }

    /**
     * Set NTP servers
     * @param primary Primary NTP server
     * @param secondary Secondary NTP server (optional)
     * @param tertiary Tertiary NTP server (optional)
     */
    void setNtpServers(const std::string& primary,
                       const std::string& secondary = "",
                       const std::string& tertiary = "");

    /**
     * Enable/disable automatic periodic sync
     * @param enabled Enable auto-sync
     * @param interval_hours Sync interval in hours (default: 1 hour)
     */
    void setAutoSync(bool enabled, uint32_t interval_hours = 1);

    /**
     * Get sync status information
     */
    struct SyncStatus {
        bool synchronized = false;
        time_t last_sync = 0;
        uint32_t sync_count = 0;
        uint32_t sync_failures = 0;
        std::string ntp_server;
        int32_t time_offset_ms = 0;  // Offset from NTP server (for drift monitoring)
    };

    SyncStatus getSyncStatus() const;

private:
    TimeManager();
    ~TimeManager();
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    static void syncTask(void* param);
    void performSync();
    bool waitForSync(uint32_t timeout_ms);

    bool initialized_ = false;
    bool synchronized_ = false;
    time_t last_sync_time_ = 0;
    uint32_t sync_count_ = 0;
    uint32_t sync_failures_ = 0;

    std::string timezone_ = "UTC0";  // Default UTC
    std::string ntp_server_primary_ = "pool.ntp.org";
    std::string ntp_server_secondary_ = "time.google.com";
    std::string ntp_server_tertiary_ = "time.cloudflare.com";

    bool auto_sync_enabled_ = true;
    uint32_t auto_sync_interval_hours_ = 1;

    TaskHandle_t sync_task_handle_ = nullptr;
    mutable SemaphoreHandle_t mutex_;

    // Default timezone strings for common regions
    static constexpr const char* TZ_UTC = "UTC0";
    static constexpr const char* TZ_EUROPE_ROME = "CET-1CEST,M3.5.0,M10.5.0/3";
    static constexpr const char* TZ_EUROPE_LONDON = "GMT0BST,M3.5.0/1,M10.5.0";
    static constexpr const char* TZ_US_EASTERN = "EST5EDT,M3.2.0,M11.1.0";
    static constexpr const char* TZ_US_PACIFIC = "PST8PDT,M3.2.0,M11.1.0";
    static constexpr const char* TZ_ASIA_TOKYO = "JST-9";
};

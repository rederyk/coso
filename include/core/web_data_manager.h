#pragma once

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "utils/psram_allocator.h"

#include <string>
#include <vector>
#include <set>
#include <map>
#include <atomic>
#include <functional>

class WebDataManager {
public:
    /** Request result structure */
    struct RequestResult {
        bool success;
        int http_code;
        std::string error_message;
        size_t bytes_received;
    };

    /** Scheduled task info */
    struct ScheduledTask {
        std::string url;
        std::string filename;
        uint32_t interval_minutes;
        uint32_t last_run;
        lv_timer_t* timer;
    };

    static WebDataManager& getInstance();

    bool begin();
    void end();

    /** Download methods */
    RequestResult fetchOnce(const std::string& url, const std::string& filename);
    bool fetchScheduled(const std::string& url, const std::string& filename, uint32_t interval_minutes);
    void notifyWifiReady();
    void notifyWifiDisconnected();

    /** Data access */
    std::string readData(const std::string& filename);
    std::vector<std::string> listFiles();
    bool deleteData(const std::string& filename);

    /** Security management */
    void addAllowedDomain(const std::string& domain);
    void removeAllowedDomain(const std::string& domain);
    bool isDomainAllowed(const std::string& url) const;

    /** Management */
    void cancelScheduled(const std::string& filename);
    void cleanupOldFiles(uint32_t max_age_hours = 24);
    std::vector<std::string> getScheduledTasks() const;

    /** Configuration */
    void loadAllowedDomainsFromSettings();
    void saveScheduledTasks();
    void loadScheduledTasks();
    void setMaxFileSize(size_t max_bytes) { max_file_size_ = max_bytes; }
    void setTimeoutMs(uint32_t timeout) { request_timeout_ms_ = timeout; }
    void setMaxRequestsPerHour(uint32_t max_requests) { max_requests_per_hour_ = max_requests; }

private:
    using ByteBuffer = PsramVector<uint8_t>;

    WebDataManager();
    ~WebDataManager();
    WebDataManager(const WebDataManager&) = delete;
    WebDataManager& operator=(const WebDataManager&) = delete;

    /** HTTP implementation */
    RequestResult makeHttpRequest(const std::string& url, ByteBuffer& response_data);
    bool validateUrl(const std::string& url) const;
    std::string extractDomain(const std::string& url) const;

    /** Scheduling */
    static void scheduledDownloadTimer(lv_timer_t* timer);
    void executeScheduledDownload(const std::string& task_id);
    void enqueueWifiPendingTask(const std::string& task_id);
    void processPendingWifiTasks();
    bool isWifiReady() const { return wifi_ready_.load(); }

    /** Rate limiting */
    bool checkRateLimit(const std::string& domain);
    void updateRateLimit(const std::string& domain);

    /** Internal state */
    bool initialized_ = false;
    // WiFiClientSecure removed - now allocated on-demand in makeHttpRequest() to save DRAM

    /** Configuration */
    size_t max_file_size_ = 50 * 1024; // 50KB default
    uint32_t request_timeout_ms_ = 10000; // 10 seconds
    uint32_t max_requests_per_hour_ = 10;

    /** Security */
    std::set<std::string> allowed_domains_;
    mutable SemaphoreHandle_t domains_mutex_;

    /** Scheduling */
    std::map<std::string, ScheduledTask> scheduled_tasks_;
    mutable SemaphoreHandle_t tasks_mutex_;
    std::set<std::string> pending_wifi_tasks_;
    std::atomic<bool> wifi_ready_{false};

    /** Rate limiting (requests per hour per domain) */
    std::map<std::string, std::vector<uint32_t>> request_timestamps_;
    mutable SemaphoreHandle_t rate_limit_mutex_;

    /** Constants */
    static constexpr const char* CACHE_DIR = "/webdata";
    static constexpr const char* LOG_TAG = "WebDataManager";
};

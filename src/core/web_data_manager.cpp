#include "core/web_data_manager.h"
#include "core/storage_access_manager.h"
#include "core/storage_manager.h"
#include "core/settings_manager.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <regex>
#include <ctime>
#include <memory>

WebDataManager& WebDataManager::getInstance() {
    static WebDataManager instance;
    return instance;
}

WebDataManager::WebDataManager() :
    domains_mutex_(xSemaphoreCreateMutex()),
    tasks_mutex_(xSemaphoreCreateMutex()),
    rate_limit_mutex_(xSemaphoreCreateMutex()) {
}

WebDataManager::~WebDataManager() {
    end();

    if (domains_mutex_) vSemaphoreDelete(domains_mutex_);
    if (tasks_mutex_) vSemaphoreDelete(tasks_mutex_);
    if (rate_limit_mutex_) vSemaphoreDelete(rate_limit_mutex_);
}

bool WebDataManager::begin() {
    if (initialized_) return true;

    ESP_LOGI(LOG_TAG, "Initializing WebDataManager...");

    // WiFiClientSecure is now created on-demand in makeHttpRequest() to save DRAM

    // Create cache directory
    if (!LittleFS.exists(CACHE_DIR)) {
        if (!LittleFS.mkdir(CACHE_DIR)) {
            ESP_LOGE(LOG_TAG, "Failed to create cache directory: %s", CACHE_DIR);
            return false;
        }
    }

    if (!StorageAccessManager::getInstance().begin()) {
        ESP_LOGE(LOG_TAG, "Failed to initialize storage access manager");
        return false;
    }

    initialized_ = true;
    wifi_ready_.store(WiFi.isConnected());

    // Load configuration from settings (after initialization)
    loadAllowedDomainsFromSettings();

    // Load persisted scheduled tasks
    loadScheduledTasks();

    ESP_LOGI(LOG_TAG, "WebDataManager initialized successfully");
    return true;
}

void WebDataManager::end() {
    if (!initialized_) return;

    ESP_LOGI(LOG_TAG, "Shutting down WebDataManager...");

    // Cancel all scheduled tasks
    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    for (auto& pair : scheduled_tasks_) {
        if (pair.second.timer) {
            lv_timer_del(pair.second.timer);
            pair.second.timer = nullptr;
        }
    }
    scheduled_tasks_.clear();
    pending_wifi_tasks_.clear();
    xSemaphoreGive(tasks_mutex_);

    initialized_ = false;
    wifi_ready_.store(false);
    ESP_LOGI(LOG_TAG, "WebDataManager shut down");
}

WebDataManager::RequestResult WebDataManager::fetchOnce(const std::string& url, const std::string& filename) {
    RequestResult result = {false, 0, "", 0};

    ESP_LOGI(LOG_TAG, "fetchOnce called: url=%s, filename=%s", url.c_str(), filename.c_str());

    if (!initialized_) {
        result.error_message = "WebDataManager not initialized";
        ESP_LOGE(LOG_TAG, "fetchOnce failed: %s", result.error_message.c_str());
        return result;
    }

    if (!WiFi.isConnected()) {
        result.error_message = "WiFi not connected";
        ESP_LOGE(LOG_TAG, "fetchOnce failed: %s", result.error_message.c_str());
        return result;
    }

    // Validate URL and domain
    if (!validateUrl(url)) {
        result.error_message = "Invalid URL format";
        ESP_LOGE(LOG_TAG, "fetchOnce failed: %s", result.error_message.c_str());
        return result;
    }

    if (!isDomainAllowed(url)) {
        result.error_message = "Domain not in whitelist";
        ESP_LOGE(LOG_TAG, "fetchOnce failed: %s (url=%s)", result.error_message.c_str(), url.c_str());
        return result;
    }

    std::string domain = extractDomain(url);
    if (!checkRateLimit(domain)) {
        result.error_message = "Rate limit exceeded for domain: " + domain;
        ESP_LOGE(LOG_TAG, "fetchOnce failed: %s", result.error_message.c_str());
        return result;
    }

    ESP_LOGI(LOG_TAG, "Fetching URL: %s -> %s", url.c_str(), filename.c_str());

    ByteBuffer response_data;
    result = makeHttpRequest(url, response_data);

    if (result.success && !response_data.empty()) {
        if (StorageAccessManager::getInstance().writeWebData(
                filename,
                response_data.data(),
                response_data.size())) {
            updateRateLimit(domain);
            ESP_LOGI(LOG_TAG, "Successfully cached %d bytes to %s", response_data.size(), filename.c_str());
        } else {
            result.success = false;
            result.error_message = "Failed to save to cache";
        }
    }

    return result;
}

bool WebDataManager::fetchScheduled(const std::string& url, const std::string& filename, uint32_t interval_minutes) {
    if (!initialized_) return false;

    std::string task_id = filename; // Use filename as task ID

    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);

    // Cancel existing task if any
    auto it = scheduled_tasks_.find(task_id);
    if (it != scheduled_tasks_.end()) {
        if (it->second.timer) {
            lv_timer_del(it->second.timer);
        }
        scheduled_tasks_.erase(it);
    }

    // Create new scheduled task
    ScheduledTask task;
    task.url = url;
    task.filename = filename;
    task.interval_minutes = interval_minutes;
    task.last_run = 0;

    // Convert minutes to milliseconds for LVGL timer
    uint32_t interval_ms = interval_minutes * 60 * 1000;

    task.timer = lv_timer_create(scheduledDownloadTimer, interval_ms, nullptr);
    if (!task.timer) {
        xSemaphoreGive(tasks_mutex_);
        ESP_LOGE(LOG_TAG, "Failed to create timer for scheduled task: %s", task_id.c_str());
        return false;
    }

    // Store task info in a global map for callback lookup
    task.timer->user_data = strdup(task_id.c_str());

    scheduled_tasks_[task_id] = task;
    xSemaphoreGive(tasks_mutex_);

    // Save tasks to persistent storage
    saveScheduledTasks();

    ESP_LOGI(LOG_TAG, "Scheduled task created: %s every %d minutes", task_id.c_str(), interval_minutes);

    // Execute immediately for first time, or queue until Wi-Fi becomes available
    if (isWifiReady()) {
        executeScheduledDownload(task_id);
    } else {
        enqueueWifiPendingTask(task_id);
        ESP_LOGI(LOG_TAG, "Wi-Fi offline, queued task %s until connection is ready", task_id.c_str());
    }

    return true;
}

std::string WebDataManager::readData(const std::string& filename) {
    if (!initialized_) return "";

    std::string data;
    if (StorageAccessManager::getInstance().readWebData(filename, data)) {
        return data;
    }
    return "";
}

std::vector<std::string> WebDataManager::listFiles() {
    if (!initialized_) {
        return {};
    }
    return StorageAccessManager::getInstance().listWebDataFiles();
}

bool WebDataManager::deleteData(const std::string& filename) {
    if (!initialized_) return false;

    if (StorageAccessManager::getInstance().deleteWebData(filename)) {
        ESP_LOGI(LOG_TAG, "Deleted cached file: %s", filename.c_str());
        return true;
    }
    return false;
}

void WebDataManager::addAllowedDomain(const std::string& domain) {
    if (!initialized_) return;

    xSemaphoreTake(domains_mutex_, portMAX_DELAY);
    allowed_domains_.insert(domain);
    xSemaphoreGive(domains_mutex_);

    ESP_LOGI(LOG_TAG, "Added allowed domain: %s", domain.c_str());
}

void WebDataManager::removeAllowedDomain(const std::string& domain) {
    if (!initialized_) return;

    xSemaphoreTake(domains_mutex_, portMAX_DELAY);
    allowed_domains_.erase(domain);
    xSemaphoreGive(domains_mutex_);

    ESP_LOGI(LOG_TAG, "Removed allowed domain: %s", domain.c_str());
}

bool WebDataManager::isDomainAllowed(const std::string& url) const {
    if (allow_all_domains_) {
        return true;
    }

    std::string domain = extractDomain(url);

    xSemaphoreTake(domains_mutex_, portMAX_DELAY);
    bool allowed = allowed_domains_.find(domain) != allowed_domains_.end();
    xSemaphoreGive(domains_mutex_);

    return allowed;
}

void WebDataManager::cancelScheduled(const std::string& filename) {
    if (!initialized_) return;

    std::string task_id = filename;

    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    auto it = scheduled_tasks_.find(task_id);
    if (it != scheduled_tasks_.end()) {
        if (it->second.timer) {
            lv_timer_del(it->second.timer);
        }
        scheduled_tasks_.erase(it);
        ESP_LOGI(LOG_TAG, "Cancelled scheduled task: %s", task_id.c_str());

        // Save updated tasks list to persistent storage
        xSemaphoreGive(tasks_mutex_);
        saveScheduledTasks();
        return;
    }
    xSemaphoreGive(tasks_mutex_);
}

void WebDataManager::cleanupOldFiles(uint32_t max_age_hours) {
    if (!initialized_) return;

    uint32_t max_age_seconds = max_age_hours * 3600;
    uint32_t current_time = millis() / 1000;

    File root = LittleFS.open(CACHE_DIR);
    if (!root || !root.isDirectory()) {
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            time_t file_time = file.getLastWrite();
            if (current_time - file_time > max_age_seconds) {
                std::string filename = file.name();
                file.close();
                if (StorageAccessManager::getInstance().deleteWebData(filename)) {
                    ESP_LOGI(LOG_TAG, "Cleaned up old file: %s", filename.c_str());
                }
                file = root.openNextFile();
                continue;
            }
        }
        file = root.openNextFile();
    }
}

std::vector<std::string> WebDataManager::getScheduledTasks() const {
    std::vector<std::string> tasks;

    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    for (const auto& pair : scheduled_tasks_) {
        tasks.push_back(pair.first);
    }
    xSemaphoreGive(tasks_mutex_);

    return tasks;
}

// Private methods implementation

WebDataManager::RequestResult WebDataManager::makeHttpRequest(const std::string& url, ByteBuffer& response_data) {
    RequestResult result = {false, 0, "", 0};
    response_data.clear();

    const bool is_https = url.rfind("https://", 0) == 0;

    // Verify WiFi is connected before attempting request
    if (!WiFi.isConnected()) {
        result.error_message = "WiFi not connected";
        ESP_LOGE(LOG_TAG, "Cannot make HTTP request: WiFi not connected");
        return result;
    }

    // Choose client type based on scheme to avoid SSL allocations when not needed
    WiFiClient plain_client;

    // Custom deleter for placement new cleanup (used only for HTTPS)
    struct WiFiClientSecureDeleter {
        void operator()(WiFiClientSecure* ptr) {
            if (ptr) {
                ptr->~WiFiClientSecure();
                heap_caps_free(const_cast<void*>(static_cast<const void*>(ptr)));
            }
        }
    };
    std::unique_ptr<WiFiClientSecure, WiFiClientSecureDeleter> wifi_client(nullptr);

    WiFiClient* http_client = &plain_client;

    if (is_https) {
        // Create WiFiClientSecure on-demand using PSRAM allocation
        // WiFiClientSecure is large (~16-32KB), must use PSRAM to avoid DRAM exhaustion
        const size_t wifi_size = sizeof(WiFiClientSecure);
        void* mem = heap_caps_malloc(wifi_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!mem) {
            // Fallback to internal heap if PSRAM fails (though unlikely)
            mem = heap_caps_malloc(wifi_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!mem) {
                result.error_message = "Failed to allocate WiFiClientSecure (DRAM/PSRAM)";
                ESP_LOGE(LOG_TAG, "Out of memory: cannot allocate %zu bytes for WiFiClientSecure", wifi_size);
                return result;
            }
        }

        WiFiClientSecure* raw_client = new (mem) WiFiClientSecure();
        wifi_client.reset(raw_client);

        if (!wifi_client) {
            heap_caps_free(mem);
            result.error_message = "Failed to construct WiFiClientSecure";
            ESP_LOGE(LOG_TAG, "Failed to construct WiFiClientSecure");
            return result;
        }

        ESP_LOGI(LOG_TAG, "WiFiClientSecure allocated from %s (%zu bytes)",
                 (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0 ? "PSRAM" : "DRAM"), wifi_size);

        // IMPORTANT: For HTTPS to work properly:
        // 1. Use setInsecure() for development/testing (skips certificate validation)
        // 2. OR use setCACert() with proper root CA certificate for production
        // Note: RTC time must be set (via NTP) for certificate validation to work
        wifi_client->setInsecure(); // Skip certificate validation - consider using setCACert() for production
        http_client = wifi_client.get();
    }

    HTTPClient http;
    http.setTimeout(request_timeout_ms_);

    ESP_LOGI(LOG_TAG, "Making HTTP request to: %s", url.c_str());

    if (!http.begin(*http_client, url.c_str())) {
        result.error_message = "Failed to begin HTTP connection";
        return result;
    }

    int http_code = http.GET();
    result.http_code = http_code;

    if (http_code > 0) {
        if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_MOVED_PERMANENTLY) {
            WiFiClient* stream = http.getStreamPtr();
            if (!stream) {
                result.error_message = "HTTP stream unavailable";
            } else {
                constexpr size_t kChunkSize = 1024;
                auto chunk_deleter = [](uint8_t* ptr) {
                    if (ptr) {
                        heap_caps_free(ptr);
                    }
                };

                uint8_t* chunk_ptr = static_cast<uint8_t*>(heap_caps_malloc(kChunkSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                if (!chunk_ptr) {
                    chunk_ptr = static_cast<uint8_t*>(heap_caps_malloc(kChunkSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
                }
                std::unique_ptr<uint8_t, decltype(chunk_deleter)> chunk(chunk_ptr, chunk_deleter);

                if (!chunk_ptr) {
                    result.error_message = "Failed to allocate HTTP buffer";
                } else {
                    int remaining = http.getSize();
                    const bool is_chunked = (remaining == -1);
                    bool stream_error = false;

                    while ((is_chunked && stream->connected()) || (!is_chunked && remaining > 0)) {
                        size_t available = stream->available();
                        if (available == 0) {
                            if (!stream->connected()) {
                                break;
                            }
                            vTaskDelay(pdMS_TO_TICKS(10));
                            continue;
                        }

                        size_t to_read = std::min(available, kChunkSize);
                        int bytes_read = stream->readBytes(chunk.get(), to_read);
                        if (bytes_read <= 0) {
                            stream_error = true;
                            result.error_message = "HTTP stream read failed";
                            break;
                        }

                        if (response_data.size() + static_cast<size_t>(bytes_read) > max_file_size_) {
                            stream_error = true;
                            result.error_message = "Response too large: " +
                                std::to_string(response_data.size() + static_cast<size_t>(bytes_read)) + " bytes";
                            break;
                        }

                        response_data.insert(response_data.end(), chunk.get(), chunk.get() + bytes_read);

                        if (!is_chunked) {
                            remaining -= bytes_read;
                            if (remaining <= 0) {
                                break;
                            }
                        }
                    }

                    if (!stream_error) {
                        if (!is_chunked && remaining > 0) {
                            result.error_message = "Connection closed before full response";
                        } else {
                            result.success = true;
                            result.bytes_received = response_data.size();
                            ESP_LOGI(LOG_TAG, "HTTP request successful: %d bytes received", result.bytes_received);
                        }
                    }
                }
            }
        } else {
            result.error_message = "HTTP error: " + std::to_string(http_code);
        }
    } else {
        result.error_message = "HTTP request failed: " + std::to_string(http_code);
    }

    http.end();
    return result;
}

bool WebDataManager::validateUrl(const std::string& url) const {
    // Basic URL validation - must start with http:// or https://
    if (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://") {
        return false;
    }

    // Check for basic URL structure
    size_t double_slash = url.find("://");
    if (double_slash == std::string::npos) return false;

    size_t first_slash = url.find("/", double_slash + 3);
    if (first_slash == std::string::npos) return false;

    return true;
}

std::string WebDataManager::extractDomain(const std::string& url) const {
    size_t double_slash = url.find("://");
    if (double_slash == std::string::npos) return "";

    size_t start = double_slash + 3;
    size_t end = url.find("/", start);
    if (end == std::string::npos) end = url.length();

    std::string domain = url.substr(start, end - start);

    // Remove port if present
    size_t colon = domain.find(":");
    if (colon != std::string::npos) {
        domain = domain.substr(0, colon);
    }

    return domain;
}

void WebDataManager::scheduledDownloadTimer(lv_timer_t* timer) {
    if (!timer) {
        ESP_LOGE(LOG_TAG, "scheduledDownloadTimer called with null timer");
        return;
    }

    if (!timer->user_data) {
        ESP_LOGE(LOG_TAG, "scheduledDownloadTimer called with null user_data");
        return;
    }

    char* task_id = (char*)timer->user_data;

    // Verify WebDataManager is still initialized
    WebDataManager& wdm = WebDataManager::getInstance();
    if (!wdm.initialized_) {
        ESP_LOGW(LOG_TAG, "scheduledDownloadTimer called but WebDataManager not initialized");
        return;
    }

    try {
        wdm.executeScheduledDownload(task_id);
    } catch (const std::exception& e) {
        ESP_LOGE(LOG_TAG, "Exception in scheduledDownloadTimer for task %s: %s", task_id, e.what());
    } catch (...) {
        ESP_LOGE(LOG_TAG, "Unknown exception in scheduledDownloadTimer for task %s", task_id);
    }
}

void WebDataManager::enqueueWifiPendingTask(const std::string& task_id) {
    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    if (scheduled_tasks_.find(task_id) != scheduled_tasks_.end()) {
        pending_wifi_tasks_.insert(task_id);
    }
    xSemaphoreGive(tasks_mutex_);
}

void WebDataManager::processPendingWifiTasks() {
    if (!initialized_) {
        return;
    }

    std::vector<std::string> tasks_to_run;
    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    tasks_to_run.assign(pending_wifi_tasks_.begin(), pending_wifi_tasks_.end());
    pending_wifi_tasks_.clear();
    xSemaphoreGive(tasks_mutex_);

    for (const auto& task_id : tasks_to_run) {
        executeScheduledDownload(task_id);
    }
}

void WebDataManager::notifyWifiReady() {
    wifi_ready_.store(true);
    processPendingWifiTasks();
}

void WebDataManager::notifyWifiDisconnected() {
    wifi_ready_.store(false);
}

void WebDataManager::executeScheduledDownload(const std::string& task_id) {
    ScheduledTask task;

    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    auto it = scheduled_tasks_.find(task_id);
    if (it == scheduled_tasks_.end()) {
        xSemaphoreGive(tasks_mutex_);
        return;
    }

    if (!isWifiReady()) {
        pending_wifi_tasks_.insert(task_id);
        xSemaphoreGive(tasks_mutex_);
        ESP_LOGW(LOG_TAG, "Wi-Fi unavailable, deferring scheduled download: %s", task_id.c_str());
        return;
    }

    task = it->second;
    xSemaphoreGive(tasks_mutex_);

    ESP_LOGI(LOG_TAG, "Executing scheduled download: %s", task_id.c_str());

    // Execute the download
    fetchOnce(task.url, task.filename);

    // Update last run time
    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    task.last_run = millis() / 1000;
    xSemaphoreGive(tasks_mutex_);
}

bool WebDataManager::checkRateLimit(const std::string& domain) {
    xSemaphoreTake(rate_limit_mutex_, portMAX_DELAY);

    auto it = request_timestamps_.find(domain);
    if (it == request_timestamps_.end()) {
        xSemaphoreGive(rate_limit_mutex_);
        ESP_LOGI(LOG_TAG, "Rate limit check for domain %s: no requests yet, allowed", domain.c_str());
        return true; // No requests yet, allow
    }

    auto& timestamps = it->second;
    uint32_t current_time = millis() / 1000;
    uint32_t one_hour_ago = current_time - 3600;

    // Remove old timestamps (older than 1 hour)
    size_t old_count = timestamps.size();
    auto new_end = std::remove_if(timestamps.begin(), timestamps.end(),
        [one_hour_ago](uint32_t ts) { return ts < one_hour_ago; });
    timestamps.erase(new_end, timestamps.end());
    size_t cleaned_count = old_count - timestamps.size();

    if (cleaned_count > 0) {
        ESP_LOGI(LOG_TAG, "Rate limit cleanup for domain %s: removed %d old timestamps", domain.c_str(), cleaned_count);
    }

    bool allowed = timestamps.size() < max_requests_per_hour_;
    if (!allowed) {
        ESP_LOGW(LOG_TAG, "Rate limit exceeded for domain %s: %d/%d requests in last hour",
                 domain.c_str(), timestamps.size(), max_requests_per_hour_);
    } else {
        ESP_LOGI(LOG_TAG, "Rate limit check for domain %s: %d/%d requests, allowed",
                 domain.c_str(), timestamps.size(), max_requests_per_hour_);
    }

    xSemaphoreGive(rate_limit_mutex_);
    return allowed;
}

void WebDataManager::updateRateLimit(const std::string& domain) {
    xSemaphoreTake(rate_limit_mutex_, portMAX_DELAY);

    request_timestamps_[domain].push_back(millis() / 1000);

    xSemaphoreGive(rate_limit_mutex_);
}

void WebDataManager::loadAllowedDomainsFromSettings() {
    if (!initialized_) return;

    SettingsManager& settings = SettingsManager::getInstance();
    const auto& allowed_domains = settings.getWebDataAllowedDomains();

    xSemaphoreTake(domains_mutex_, portMAX_DELAY);
    allowed_domains_.clear();
    for (const auto& domain : allowed_domains) {
        allowed_domains_.insert(domain);
        ESP_LOGI(LOG_TAG, "Loaded allowed domain from settings: %s", domain.c_str());
    }
    xSemaphoreGive(domains_mutex_);

    // Also load other configuration from settings
    max_file_size_ = settings.getWebDataMaxFileSizeKb() * 1024; // Convert KB to bytes
    max_requests_per_hour_ = settings.getWebDataMaxRequestsPerHour();
    request_timeout_ms_ = settings.getWebDataRequestTimeoutMs();

    ESP_LOGI(LOG_TAG, "Configuration loaded from settings: max_file_size=%dKB, max_requests_per_hour=%d, timeout=%dms",
             settings.getWebDataMaxFileSizeKb(), max_requests_per_hour_, request_timeout_ms_);
}

void WebDataManager::saveScheduledTasks() {
    if (!initialized_) return;

    std::string tasks_file = "/webdata/scheduled_tasks.json";

    // Create JSON document
    DynamicJsonDocument doc(2048);

    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    for (const auto& pair : scheduled_tasks_) {
        const std::string& task_id = pair.first;
        const ScheduledTask& task = pair.second;

        JsonObject task_obj = doc.createNestedObject(task_id);
        task_obj["url"] = task.url;
        task_obj["filename"] = task.filename;
        task_obj["interval_minutes"] = task.interval_minutes;
        task_obj["last_run"] = task.last_run;
    }
    xSemaphoreGive(tasks_mutex_);

    // Save to file
    File file = LittleFS.open(tasks_file.c_str(), "w");
    if (!file) {
        ESP_LOGE(LOG_TAG, "Failed to open scheduled tasks file for writing");
        return;
    }

    if (serializeJson(doc, file) == 0) {
        ESP_LOGE(LOG_TAG, "Failed to serialize scheduled tasks to JSON");
    } else {
        ESP_LOGI(LOG_TAG, "Saved %d scheduled tasks to file", doc.size());
    }

    file.close();
}

void WebDataManager::loadScheduledTasks() {
    if (!initialized_) return;

    std::string tasks_file = "/webdata/scheduled_tasks.json";

    File file = LittleFS.open(tasks_file.c_str(), "r");
    if (!file) {
        ESP_LOGI(LOG_TAG, "No scheduled tasks file found, starting with empty tasks");
        return;
    }

    // Parse JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        ESP_LOGE(LOG_TAG, "Failed to parse scheduled tasks JSON: %s", error.c_str());
        return;
    }

    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);
    int loaded_count = 0;
    for (JsonPair kv : doc.as<JsonObject>()) {
        const char* task_id = kv.key().c_str();
        JsonObject task_obj = kv.value();

        ScheduledTask task;
        task.url = task_obj["url"] | "";
        task.filename = task_obj["filename"] | "";
        task.interval_minutes = task_obj["interval_minutes"] | 0;
        task.last_run = task_obj["last_run"] | 0;
        task.timer = nullptr; // Will be recreated

        if (!task.url.empty() && !task.filename.empty() && task.interval_minutes > 0) {
            // Recreate the timer
            uint32_t interval_ms = task.interval_minutes * 60 * 1000;
            task.timer = lv_timer_create(scheduledDownloadTimer, interval_ms, nullptr);
            if (task.timer) {
                task.timer->user_data = strdup(task.filename.c_str());
                scheduled_tasks_[task.filename] = task;
                loaded_count++;
                ESP_LOGI(LOG_TAG, "Restored scheduled task: %s", task.filename.c_str());
            } else {
                ESP_LOGE(LOG_TAG, "Failed to recreate timer for task: %s", task.filename.c_str());
            }
        }
    }
    xSemaphoreGive(tasks_mutex_);

    ESP_LOGI(LOG_TAG, "Loaded %d scheduled tasks from file", loaded_count);
}

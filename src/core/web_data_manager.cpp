#include "core/web_data_manager.h"
#include "core/storage_manager.h"
#include "core/settings_manager.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_log.h>

#include <algorithm>
#include <regex>
#include <ctime>

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

    // Initialize WiFiClientSecure
    wifi_client_.setInsecure(); // For development - in production use proper certificates

    // Create cache directory
    if (!LittleFS.exists(CACHE_DIR)) {
        if (!LittleFS.mkdir(CACHE_DIR)) {
            ESP_LOGE(LOG_TAG, "Failed to create cache directory: %s", CACHE_DIR);
            return false;
        }
    }

    initialized_ = true;

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
    xSemaphoreGive(tasks_mutex_);

    initialized_ = false;
    ESP_LOGI(LOG_TAG, "WebDataManager shut down");
}

WebDataManager::RequestResult WebDataManager::fetchOnce(const std::string& url, const std::string& filename) {
    RequestResult result = {false, 0, "", 0};

    if (!initialized_) {
        result.error_message = "WebDataManager not initialized";
        return result;
    }

    if (!WiFi.isConnected()) {
        result.error_message = "WiFi not connected";
        return result;
    }

    // Validate URL and domain
    if (!validateUrl(url)) {
        result.error_message = "Invalid URL format";
        return result;
    }

    if (!isDomainAllowed(url)) {
        result.error_message = "Domain not in whitelist";
        return result;
    }

    std::string domain = extractDomain(url);
    if (!checkRateLimit(domain)) {
        result.error_message = "Rate limit exceeded for domain: " + domain;
        return result;
    }

    ESP_LOGI(LOG_TAG, "Fetching URL: %s -> %s", url.c_str(), filename.c_str());

    std::string response_data;
    result = makeHttpRequest(url, response_data);

    if (result.success && !response_data.empty()) {
        if (saveToCache(filename, response_data)) {
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

    // Execute immediately for first time
    executeScheduledDownload(task_id);

    return true;
}

std::string WebDataManager::readData(const std::string& filename) {
    if (!initialized_) return "";

    std::string data;
    if (loadFromCache(filename, data)) {
        return data;
    }
    return "";
}

std::vector<std::string> WebDataManager::listFiles() {
    std::vector<std::string> files;

    if (!initialized_) return files;

    File root = LittleFS.open(CACHE_DIR);
    if (!root || !root.isDirectory()) {
        return files;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            files.push_back(file.name());
        }
        file = root.openNextFile();
    }

    return files;
}

bool WebDataManager::deleteData(const std::string& filename) {
    if (!initialized_) return false;

    std::string path = getCachePath(filename);
    if (LittleFS.remove(path.c_str())) {
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
                if (LittleFS.remove(filename.c_str())) {
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

WebDataManager::RequestResult WebDataManager::makeHttpRequest(const std::string& url, std::string& response_data) {
    RequestResult result = {false, 0, "", 0};

    HTTPClient http;
    http.setTimeout(request_timeout_ms_);

    ESP_LOGI(LOG_TAG, "Making HTTP request to: %s", url.c_str());

    if (!http.begin(wifi_client_, url.c_str())) {
        result.error_message = "Failed to begin HTTP connection";
        return result;
    }

    int http_code = http.GET();
    result.http_code = http_code;

    if (http_code > 0) {
        if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_MOVED_PERMANENTLY) {
            response_data = http.getString().c_str();

            // Check size limit
            if (response_data.size() > max_file_size_) {
                result.error_message = "Response too large: " + std::to_string(response_data.size()) + " bytes";
                return result;
            }

            result.success = true;
            result.bytes_received = response_data.size();
            ESP_LOGI(LOG_TAG, "HTTP request successful: %d bytes received", result.bytes_received);
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

bool WebDataManager::saveToCache(const std::string& filename, const std::string& data) {
    std::string path = getCachePath(filename);

    File file = LittleFS.open(path.c_str(), "w");
    if (!file) {
        ESP_LOGE(LOG_TAG, "Failed to open file for writing: %s", path.c_str());
        return false;
    }

    size_t written = file.write((uint8_t*)data.c_str(), data.size());
    file.close();

    if (written != data.size()) {
        ESP_LOGE(LOG_TAG, "Failed to write all data: %d/%d bytes", written, data.size());
        return false;
    }

    return true;
}

bool WebDataManager::loadFromCache(const std::string& filename, std::string& data) {
    std::string path = getCachePath(filename);

    File file = LittleFS.open(path.c_str(), "r");
    if (!file) {
        return false;
    }

    size_t size = file.size();
    if (size == 0) {
        file.close();
        return false;
    }

    data.resize(size);
    size_t read = file.read((uint8_t*)data.data(), size);
    file.close();

    if (read != size) {
        ESP_LOGE(LOG_TAG, "Failed to read all data: %d/%d bytes", read, size);
        return false;
    }

    return true;
}

std::string WebDataManager::getCachePath(const std::string& filename) const {
    return std::string(CACHE_DIR) + "/" + filename;
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

void WebDataManager::executeScheduledDownload(const std::string& task_id) {
    xSemaphoreTake(tasks_mutex_, portMAX_DELAY);

    auto it = scheduled_tasks_.find(task_id);
    if (it == scheduled_tasks_.end()) {
        xSemaphoreGive(tasks_mutex_);
        return;
    }

    ScheduledTask& task = it->second;
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

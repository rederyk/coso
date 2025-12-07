#include "core/time_scheduler.h"
#include "core/storage_access_manager.h"
#include "core/command_center.h"
#include "core/time_manager.h"
#include "utils/logger.h"

#include <Arduino.h>
#include <cJSON.h>
#include <esp_random.h>

// Singleton instance
TimeScheduler& TimeScheduler::getInstance() {
    static TimeScheduler instance;
    return instance;
}

TimeScheduler::TimeScheduler()
    : initialized_(false)
    , enabled_(true)
    , mutex_(nullptr)
    , task_handle_(nullptr)
{
}

TimeScheduler::~TimeScheduler() {
    end();
}

bool TimeScheduler::begin() {
    if (initialized_) {
        return true;
    }

    ESP_LOGI(LOG_TAG, "Initializing TimeScheduler...");

    // Create mutex
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(LOG_TAG, "Failed to create mutex");
        return false;
    }

    // Load events from storage
    loadFromStorage();

    // Create scheduler task (Core 0, priority 2)
    BaseType_t result = xTaskCreatePinnedToCore(
        schedulerTask,
        "time_scheduler",
        4096,
        this,
        2,
        &task_handle_,
        0
    );

    if (result != pdPASS) {
        ESP_LOGE(LOG_TAG, "Failed to create scheduler task");
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
        return false;
    }

    initialized_ = true;
    ESP_LOGI(LOG_TAG, "TimeScheduler initialized with %d events", events_.size());
    return true;
}

void TimeScheduler::end() {
    if (!initialized_) {
        return;
    }

    ESP_LOGI(LOG_TAG, "Shutting down TimeScheduler...");

    // Delete task
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }

    // Save before shutdown
    saveToStorage();

    // Delete mutex
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }

    initialized_ = false;
}

bool TimeScheduler::isRunning() const {
    return initialized_ && task_handle_ != nullptr;
}

void TimeScheduler::schedulerTask(void* param) {
    TimeScheduler* scheduler = static_cast<TimeScheduler*>(param);
    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI(LOG_TAG, "Scheduler task started");

    while (true) {
        // Check every 30 seconds
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(30000));

        if (!scheduler->enabled_) {
            continue;
        }

        // Check and run due events
        scheduler->checkAndRunEvents();
    }
}

void TimeScheduler::checkAndRunEvents() {
    if (!TimeManager::getInstance().isSynchronized()) {
        // Skip if time not synced
        return;
    }

    // Get current time
    time_t now_unix = TimeManager::getInstance().now();
    struct tm now_tm;
    localtime_r(&now_unix, &now_tm);

    xSemaphoreTake(mutex_, portMAX_DELAY);

    for (auto& pair : events_) {
        CalendarEvent& event = pair.second;

        if (!event.enabled) {
            continue;
        }

        if (event.status == EventStatus::Running) {
            continue; // Already running
        }

        if (event.type == EventType::OneShot && event.status == EventStatus::Completed) {
            continue; // One-shot already completed
        }

        // Check if should run now
        if (shouldRunNow(event, now_tm)) {
            ESP_LOGI(LOG_TAG, "Triggering event: %s", event.name.c_str());

            // Execute in separate task to avoid blocking
            xSemaphoreGive(mutex_);
            executeEvent(event);
            xSemaphoreTake(mutex_, portMAX_DELAY);
        }
    }

    xSemaphoreGive(mutex_);
}

bool TimeScheduler::shouldRunNow(const CalendarEvent& event, const struct tm& now) {
    // Check hour and minute
    if (event.hour != now.tm_hour || event.minute != now.tm_min) {
        return false;
    }

    if (event.type == EventType::OneShot) {
        // Check exact date
        return (event.year == (now.tm_year + 1900) &&
                event.month == (now.tm_mon + 1) &&
                event.day == now.tm_mday);
    } else {
        // Recurring - check weekday
        uint8_t today_bit = (1 << now.tm_wday);
        return (event.weekdays & today_bit) != 0;
    }
}

void TimeScheduler::executeEvent(CalendarEvent& event) {
    ExecutionRecord record;
    record.event_id = event.id;
    record.timestamp = TimeManager::getInstance().now();

    xSemaphoreTake(mutex_, portMAX_DELAY);
    event.status = EventStatus::Running;
    event.last_run = record.timestamp;
    xSemaphoreGive(mutex_);

    ESP_LOGI(LOG_TAG, "Executing event: %s", event.name.c_str());

    uint32_t start_ms = millis();

    // Execute Lua script via CommandCenter
    auto& cmd_center = CommandCenter::getInstance();
    std::vector<std::string> args = { event.lua_script };
    auto result = cmd_center.executeCommand("lua_script", args);

    record.duration_ms = millis() - start_ms;
    record.success = result.success;
    record.output = result.message;
    record.error = result.success ? "" : result.message;

    xSemaphoreTake(mutex_, portMAX_DELAY);

    event.execution_count++;

    if (result.success) {
        event.status = (event.type == EventType::OneShot) ? EventStatus::Completed : EventStatus::Pending;
        event.last_error = "";
        ESP_LOGI(LOG_TAG, "Event '%s' executed successfully", event.name.c_str());
    } else {
        event.status = EventStatus::Failed;
        event.last_error = result.message;
        ESP_LOGE(LOG_TAG, "Event '%s' failed: %s", event.name.c_str(), result.message.c_str());
    }

    // Calculate next run
    calculateNextRun(event);

    // Save to history
    addToHistory(record);

    // Persist changes
    xSemaphoreGive(mutex_);
    saveToStorage();
}

std::string TimeScheduler::createEvent(const CalendarEvent& event) {
    if (!validateEvent(event)) {
        ESP_LOGE(LOG_TAG, "Invalid event");
        return "";
    }

    CalendarEvent new_event = event;
    new_event.id = generateEventId();
    new_event.created_at = TimeManager::getInstance().now();
    new_event.last_run = 0;
    new_event.execution_count = 0;
    new_event.status = EventStatus::Pending;

    calculateNextRun(new_event);

    xSemaphoreTake(mutex_, portMAX_DELAY);
    events_[new_event.id] = new_event;
    xSemaphoreGive(mutex_);

    saveToStorage();

    ESP_LOGI(LOG_TAG, "Created event: %s (ID: %s)", new_event.name.c_str(), new_event.id.c_str());
    return new_event.id;
}

bool TimeScheduler::updateEvent(const std::string& id, const CalendarEvent& event) {
    if (!validateEvent(event)) {
        return false;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);

    auto it = events_.find(id);
    if (it == events_.end()) {
        xSemaphoreGive(mutex_);
        return false;
    }

    CalendarEvent updated = event;
    updated.id = id;
    updated.created_at = it->second.created_at;
    updated.last_run = it->second.last_run;
    updated.execution_count = it->second.execution_count;

    calculateNextRun(updated);
    events_[id] = updated;

    xSemaphoreGive(mutex_);
    saveToStorage();

    ESP_LOGI(LOG_TAG, "Updated event: %s", id.c_str());
    return true;
}

bool TimeScheduler::deleteEvent(const std::string& id) {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    auto it = events_.find(id);
    if (it == events_.end()) {
        xSemaphoreGive(mutex_);
        return false;
    }

    events_.erase(it);
    xSemaphoreGive(mutex_);

    saveToStorage();

    ESP_LOGI(LOG_TAG, "Deleted event: %s", id.c_str());
    return true;
}

bool TimeScheduler::enableEvent(const std::string& id, bool enabled) {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    auto it = events_.find(id);
    if (it == events_.end()) {
        xSemaphoreGive(mutex_);
        return false;
    }

    it->second.enabled = enabled;
    xSemaphoreGive(mutex_);

    saveToStorage();

    ESP_LOGI(LOG_TAG, "Event %s: %s", id.c_str(), enabled ? "enabled" : "disabled");
    return true;
}

std::vector<TimeScheduler::CalendarEvent> TimeScheduler::listEvents() const {
    std::vector<CalendarEvent> result;

    xSemaphoreTake(mutex_, portMAX_DELAY);
    for (const auto& pair : events_) {
        result.push_back(pair.second);
    }
    xSemaphoreGive(mutex_);

    return result;
}

bool TimeScheduler::getEvent(const std::string& id, CalendarEvent& event) const {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    auto it = events_.find(id);
    if (it == events_.end()) {
        xSemaphoreGive(mutex_);
        return false;
    }

    event = it->second;
    xSemaphoreGive(mutex_);
    return true;
}

std::vector<TimeScheduler::ExecutionRecord> TimeScheduler::getHistory(const std::string& event_id, uint16_t limit) const {
    std::vector<ExecutionRecord> result;

    xSemaphoreTake(mutex_, portMAX_DELAY);

    for (const auto& record : history_) {
        if (event_id.empty() || record.event_id == event_id) {
            result.push_back(record);
            if (result.size() >= limit) {
                break;
            }
        }
    }

    xSemaphoreGive(mutex_);
    return result;
}

bool TimeScheduler::executeEventNow(const std::string& id) {
    CalendarEvent event;

    xSemaphoreTake(mutex_, portMAX_DELAY);
    auto it = events_.find(id);
    if (it == events_.end()) {
        xSemaphoreGive(mutex_);
        return false;
    }
    event = it->second;
    xSemaphoreGive(mutex_);

    executeEvent(event);
    return true;
}

void TimeScheduler::setEnabled(bool enabled) {
    enabled_ = enabled;
    ESP_LOGI(LOG_TAG, "Scheduler %s", enabled ? "enabled" : "disabled");
}

void TimeScheduler::calculateNextRun(CalendarEvent& event) {
    if (event.type == EventType::OneShot) {
        // One-shot: calculate from date
        struct tm target = {0};
        target.tm_year = event.year - 1900;
        target.tm_mon = event.month - 1;
        target.tm_mday = event.day;
        target.tm_hour = event.hour;
        target.tm_min = event.minute;
        event.next_run = mktime(&target);
    } else {
        // Recurring: next weekday occurrence
        event.next_run = getNextWeekdayTime(event.hour, event.minute, event.weekdays);
    }
}

uint32_t TimeScheduler::getNextWeekdayTime(uint8_t hour, uint8_t minute, uint8_t weekdays_mask) {
    time_t now = TimeManager::getInstance().now();
    struct tm now_tm;
    localtime_r(&now, &now_tm);

    // Try next 7 days
    for (int i = 0; i < 7; i++) {
        struct tm test_tm = now_tm;
        test_tm.tm_mday += i;
        test_tm.tm_hour = hour;
        test_tm.tm_min = minute;
        test_tm.tm_sec = 0;

        time_t test_time = mktime(&test_tm);
        localtime_r(&test_time, &test_tm);

        uint8_t weekday_bit = (1 << test_tm.tm_wday);

        if ((weekdays_mask & weekday_bit) && (test_time > now)) {
            return test_time;
        }
    }

    return 0;
}

std::string TimeScheduler::generateEventId() const {
    char id[32];
    snprintf(id, sizeof(id), "evt_%08lx", (unsigned long)esp_random());
    return std::string(id);
}

bool TimeScheduler::validateEvent(const CalendarEvent& event) const {
    if (event.name.empty()) {
        return false;
    }

    if (event.hour > 23 || event.minute > 59) {
        return false;
    }

    if (event.type == EventType::OneShot) {
        if (event.year < 2024 || event.month < 1 || event.month > 12 || event.day < 1 || event.day > 31) {
            return false;
        }
    } else {
        if (event.weekdays == 0) {
            return false; // No weekdays selected
        }
    }

    if (event.lua_script.empty()) {
        return false;
    }

    return true;
}

void TimeScheduler::addToHistory(const ExecutionRecord& record) {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    history_.insert(history_.begin(), record);

    // Limit history size
    if (history_.size() > max_history_entries_) {
        history_.resize(max_history_entries_);
    }

    xSemaphoreGive(mutex_);
}

void TimeScheduler::saveToStorage() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);

    // Events array
    cJSON* events_array = cJSON_CreateArray();

    xSemaphoreTake(mutex_, portMAX_DELAY);

    for (const auto& pair : events_) {
        const CalendarEvent& event = pair.second;

        cJSON* event_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(event_obj, "id", event.id.c_str());
        cJSON_AddStringToObject(event_obj, "name", event.name.c_str());
        cJSON_AddStringToObject(event_obj, "description", event.description.c_str());
        cJSON_AddNumberToObject(event_obj, "type", static_cast<int>(event.type));
        cJSON_AddBoolToObject(event_obj, "enabled", event.enabled);
        cJSON_AddNumberToObject(event_obj, "hour", event.hour);
        cJSON_AddNumberToObject(event_obj, "minute", event.minute);
        cJSON_AddNumberToObject(event_obj, "weekdays", event.weekdays);
        cJSON_AddNumberToObject(event_obj, "year", event.year);
        cJSON_AddNumberToObject(event_obj, "month", event.month);
        cJSON_AddNumberToObject(event_obj, "day", event.day);
        cJSON_AddStringToObject(event_obj, "lua_script", event.lua_script.c_str());
        cJSON_AddNumberToObject(event_obj, "created_at", event.created_at);
        cJSON_AddNumberToObject(event_obj, "last_run", event.last_run);
        cJSON_AddNumberToObject(event_obj, "next_run", event.next_run);
        cJSON_AddNumberToObject(event_obj, "execution_count", event.execution_count);
        cJSON_AddNumberToObject(event_obj, "status", static_cast<int>(event.status));
        cJSON_AddStringToObject(event_obj, "last_error", event.last_error.c_str());

        cJSON_AddItemToArray(events_array, event_obj);
    }

    xSemaphoreGive(mutex_);

    cJSON_AddItemToObject(root, "events", events_array);

    // Convert to string
    char* json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        StorageAccessManager::getInstance().writeWebData(STORAGE_PATH, json_str);
        free(json_str);
    }

    cJSON_Delete(root);
}


void TimeScheduler::loadFromStorage() {
    std::string json_data;
    if (!StorageAccessManager::getInstance().readWebData(STORAGE_PATH, json_data)) {
        // Create default empty events file
        ESP_LOGI(LOG_TAG, "No saved events found, creating default");
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "version", 1);
        cJSON* events_array = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "events", events_array);
        char* json_str = cJSON_PrintUnformatted(root);
        if (json_str) {
            StorageAccessManager::getInstance().writeWebData(STORAGE_PATH, json_str);
            free(json_str);
        }
        cJSON_Delete(root);
        ESP_LOGI(LOG_TAG, "Created default events file");
        return;
    }

    cJSON* root = cJSON_Parse(json_data.c_str());
    if (!root) {
        ESP_LOGE(LOG_TAG, "Failed to parse events JSON");
        return;
    }

    cJSON* events_array = cJSON_GetObjectItem(root, "events");
    if (!cJSON_IsArray(events_array)) {
        cJSON_Delete(root);
        return;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    events_.clear();

    int count = cJSON_GetArraySize(events_array);
    for (int i = 0; i < count; i++) {
        cJSON* event_obj = cJSON_GetArrayItem(events_array, i);

        CalendarEvent event;
        event.id = cJSON_GetObjectItem(event_obj, "id")->valuestring;
        event.name = cJSON_GetObjectItem(event_obj, "name")->valuestring;

        cJSON* desc = cJSON_GetObjectItem(event_obj, "description");
        event.description = desc ? desc->valuestring : "";

        event.type = static_cast<EventType>(cJSON_GetObjectItem(event_obj, "type")->valueint);
        event.enabled = cJSON_IsTrue(cJSON_GetObjectItem(event_obj, "enabled"));
        event.hour = cJSON_GetObjectItem(event_obj, "hour")->valueint;
        event.minute = cJSON_GetObjectItem(event_obj, "minute")->valueint;
        event.weekdays = cJSON_GetObjectItem(event_obj, "weekdays")->valueint;
        event.year = cJSON_GetObjectItem(event_obj, "year")->valueint;
        event.month = cJSON_GetObjectItem(event_obj, "month")->valueint;
        event.day = cJSON_GetObjectItem(event_obj, "day")->valueint;
        event.lua_script = cJSON_GetObjectItem(event_obj, "lua_script")->valuestring;
        event.created_at = cJSON_GetObjectItem(event_obj, "created_at")->valueint;
        event.last_run = cJSON_GetObjectItem(event_obj, "last_run")->valueint;
        event.next_run = cJSON_GetObjectItem(event_obj, "next_run")->valueint;
        event.execution_count = cJSON_GetObjectItem(event_obj, "execution_count")->valueint;
        event.status = static_cast<EventStatus>(cJSON_GetObjectItem(event_obj, "status")->valueint);

        cJSON* error = cJSON_GetObjectItem(event_obj, "last_error");
        event.last_error = error ? error->valuestring : "";

        events_[event.id] = event;
    }

    xSemaphoreGive(mutex_);

    cJSON_Delete(root);
    ESP_LOGI(LOG_TAG, "Loaded %d events from storage", events_.size());
}

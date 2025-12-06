#include "core/async_request_manager.h"

#include <cstdio>
#include <new>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/settings_manager.h"
#include "core/task_config.h"
#include "core/voice_assistant.h"
#include "lvgl_power_manager.h"
#include "utils/logger.h"

#define LOG_TAG "AsyncRequestManager"
#define LOG_I(...) Logger::getInstance().infof("[" LOG_TAG "] " __VA_ARGS__)
#define LOG_W(...) Logger::getInstance().warnf("[" LOG_TAG "] " __VA_ARGS__)
#define LOG_E(...) Logger::getInstance().errorf("[" LOG_TAG "] " __VA_ARGS__)

AsyncRequestManager& AsyncRequestManager::getInstance() {
    static AsyncRequestManager instance;
    return instance;
}

AsyncRequestManager::AsyncRequestManager()
    : request_queue_(nullptr),
      worker_task_(nullptr),
      cleanup_task_(nullptr) {
}

AsyncRequestManager::~AsyncRequestManager() {
    end();
}

bool AsyncRequestManager::begin() {
    if (running_) {
        LOG_W("Already running");
        return true;
    }

    LOG_I("Starting AsyncRequestManager...");

    // Create request queue
    request_queue_ = xQueueCreate(MAX_PENDING_REQUESTS, sizeof(PendingRequest*));
    if (!request_queue_) {
        LOG_E("Failed to create request queue");
        return false;
    }

    running_ = true;

    // Create worker task
    BaseType_t result = xTaskCreatePinnedToCore(
        workerTask,
        "async_worker",
        TaskConfig::VOICE_ASSISTANT_STACK_SIZE,
        this,
        TaskConfig::VOICE_ASSISTANT_PRIORITY,
        &worker_task_,
        TaskConfig::VOICE_ASSISTANT_CORE
    );

    if (result != pdPASS) {
        LOG_E("Failed to create worker task");
        running_ = false;
        vQueueDelete(request_queue_);
        request_queue_ = nullptr;
        return false;
    }

    // Create cleanup task
    result = xTaskCreatePinnedToCore(
        cleanupTask,
        "async_cleanup",
        2048,  // Small stack for cleanup
        this,
        1,  // Low priority
        &cleanup_task_,
        TaskConfig::VOICE_ASSISTANT_CORE
    );

    if (result != pdPASS) {
        LOG_E("Failed to create cleanup task");
        running_ = false;
        vTaskDelete(worker_task_);
        worker_task_ = nullptr;
        vQueueDelete(request_queue_);
        request_queue_ = nullptr;
        return false;
    }

    LOG_I("Started successfully");
    return true;
}

void AsyncRequestManager::end() {
    if (!running_) {
        return;
    }

    LOG_I("Stopping...");
    running_ = false;

    const TickType_t wait_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
    while ((worker_task_ != nullptr || cleanup_task_ != nullptr) &&
           xTaskGetTickCount() < wait_deadline) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (worker_task_ != nullptr) {
        vTaskDelete(worker_task_);
        worker_task_ = nullptr;
    }

    if (cleanup_task_ != nullptr) {
        vTaskDelete(cleanup_task_);
        cleanup_task_ = nullptr;
    }

    // Clear queue and free pending messages
    if (request_queue_) {
        PendingRequest* pending = nullptr;
        while (xQueueReceive(request_queue_, &pending, 0) == pdPASS) {
            delete pending;
        }
        vQueueDelete(request_queue_);
        request_queue_ = nullptr;
    }

    // Clear results
    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_.clear();
    }

    LOG_I("Stopped");
}

bool AsyncRequestManager::submitRequest(const std::string& message, std::string& request_id) {
    if (!running_ || request_queue_ == nullptr) {
        LOG_E("Not running");
        return false;
    }

    if (message.empty()) {
        LOG_E("Empty message");
        return false;
    }

    // Check if voice assistant is enabled
    SettingsManager& settings = SettingsManager::getInstance();
    if (!settings.getVoiceAssistantEnabled()) {
        LOG_E("Voice assistant is disabled");
        return false;
    }

    // Generate unique request ID
    request_id = generateRequestId();

    // Create pending request
    PendingRequest* req = new (std::nothrow) PendingRequest();
    if (!req) {
        LOG_E("Failed to allocate pending request");
        return false;
    }
    req->request_id = request_id;
    req->message = message;
    req->created_at_ms = getCurrentTimeMs();

    // Create initial result entry
    {
        std::lock_guard<std::mutex> lock(results_mutex_);

        // Check if we have too many stored results
        if (results_.size() >= MAX_STORED_RESULTS) {
            LOG_W("Too many stored results, cleaning oldest...");
            // This should be rare due to cleanup task, but handle it anyway
            auto oldest = results_.begin();
            for (auto it = results_.begin(); it != results_.end(); ++it) {
                if (it->second.created_at_ms < oldest->second.created_at_ms) {
                    oldest = it;
                }
            }
            results_.erase(oldest);
        }

        RequestResult result;
        result.status = RequestStatus::PENDING;
        result.created_at_ms = req->created_at_ms;
        result.completed_at_ms = 0;
        results_[request_id] = result;
    }

    // Queue the request
    if (xQueueSend(request_queue_, &req, 0) != pdPASS) {
        LOG_E("Failed to queue request (queue full)");

        // Remove from results
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_.erase(request_id);
        delete req;
        return false;
    }

    LOG_I("Request %s queued: %s", request_id.c_str(), message.c_str());
    return true;
}

bool AsyncRequestManager::getRequestStatus(const std::string& request_id, RequestResult& result) {
    std::lock_guard<std::mutex> lock(results_mutex_);

    auto it = results_.find(request_id);
    if (it == results_.end()) {
        return false;
    }

    result = it->second;
    return true;
}

bool AsyncRequestManager::cancelRequest(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(results_mutex_);

    auto it = results_.find(request_id);
    if (it == results_.end()) {
        return false;
    }

    // Can only cancel pending or processing requests
    if (it->second.status != RequestStatus::PENDING &&
        it->second.status != RequestStatus::PROCESSING) {
        return false;
    }

    it->second.status = RequestStatus::FAILED;
    it->second.error_message = "Cancelled by user";
    it->second.completed_at_ms = getCurrentTimeMs();

    LOG_I("Request %s cancelled", request_id.c_str());
    return true;
}

size_t AsyncRequestManager::getPendingCount() const {
    if (!request_queue_) {
        return 0;
    }
    return uxQueueMessagesWaiting(request_queue_);
}

size_t AsyncRequestManager::getProcessingCount() const {
    std::lock_guard<std::mutex> lock(results_mutex_);

    size_t count = 0;
    for (const auto& pair : results_) {
        if (pair.second.status == RequestStatus::PROCESSING) {
            count++;
        }
    }
    return count;
}

void AsyncRequestManager::workerTask(void* param) {
    AsyncRequestManager* manager = static_cast<AsyncRequestManager*>(param);
    PendingRequest* request = nullptr;

    LOG_I("Worker task started");

    while (manager->running_) {
        // Wait for a request (blocking with timeout)
        if (xQueueReceive(manager->request_queue_, &request, pdMS_TO_TICKS(1000)) == pdPASS) {
            if (request) {
                LOG_I("Processing request %s", request->request_id.c_str());
                manager->processRequest(*request);
                delete request;
                request = nullptr;
            }
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    LOG_I("Worker task stopped");
    manager->worker_task_ = nullptr;
    vTaskDelete(nullptr);
}

void AsyncRequestManager::cleanupTask(void* param) {
    AsyncRequestManager* manager = static_cast<AsyncRequestManager*>(param);

    LOG_I("Cleanup task started");

    while (manager->running_) {
        vTaskDelay(pdMS_TO_TICKS(CLEANUP_INTERVAL_MS));

        std::lock_guard<std::mutex> lock(manager->results_mutex_);

        uint64_t now = manager->getCurrentTimeMs();
        auto it = manager->results_.begin();

        while (it != manager->results_.end()) {
            const RequestResult& result = it->second;
            bool should_remove = false;

            // Remove completed/failed requests older than TTL
            if ((result.status == RequestStatus::COMPLETED ||
                 result.status == RequestStatus::FAILED ||
                 result.status == RequestStatus::TIMEOUT) &&
                result.completed_at_ms > 0 &&
                (now - result.completed_at_ms) > COMPLETED_REQUEST_TTL_MS) {
                should_remove = true;
                LOG_I("Removing expired request %s", it->first.c_str());
            }

            // Timeout stuck requests
            if ((result.status == RequestStatus::PENDING ||
                 result.status == RequestStatus::PROCESSING) &&
                (now - result.created_at_ms) > REQUEST_TIMEOUT_MS) {
                LOG_W("Request %s timed out", it->first.c_str());
                it->second.status = RequestStatus::TIMEOUT;
                it->second.error_message = "Request processing timeout";
                it->second.completed_at_ms = now;
            }

            if (should_remove) {
                it = manager->results_.erase(it);
            } else {
                ++it;
            }
        }
    }

    LOG_I("Cleanup task stopped");
    manager->cleanup_task_ = nullptr;
    vTaskDelete(nullptr);
}

void AsyncRequestManager::processRequest(const PendingRequest& request) {
    // Update status to processing
    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        auto it = results_.find(request.request_id);
        if (it != results_.end()) {
            it->second.status = RequestStatus::PROCESSING;
        }
    }

    // Initialize voice assistant if needed
    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    if (!assistant.isInitialized()) {
        LOG_I("Initializing VoiceAssistant for request %s", request.request_id.c_str());

        // Suspend LVGL to free DRAM
        LVGLPowerMgr.switchToVoiceMode();
        vTaskDelay(pdMS_TO_TICKS(100));

        if (!assistant.begin()) {
            LOG_E("Failed to initialize VoiceAssistant");

            // Resume LVGL on failure
            LVGLPowerMgr.switchToUIMode();

            // Mark as failed
            std::lock_guard<std::mutex> lock(results_mutex_);
            auto it = results_.find(request.request_id);
            if (it != results_.end()) {
                it->second.status = RequestStatus::FAILED;
                it->second.error_message = "Voice assistant initialization failed";
                it->second.completed_at_ms = getCurrentTimeMs();
            }
            return;
        }

        LOG_I("VoiceAssistant initialized successfully");
    }

    // Send message to voice assistant
    if (!assistant.sendTextMessage(request.message)) {
        LOG_E("Failed to send message to VoiceAssistant");

        std::lock_guard<std::mutex> lock(results_mutex_);
        auto it = results_.find(request.request_id);
        if (it != results_.end()) {
            it->second.status = RequestStatus::FAILED;
            it->second.error_message = "Failed to send message to voice assistant";
            it->second.completed_at_ms = getCurrentTimeMs();
        }
        return;
    }

    // Wait for response (with no timeout - handled by cleanup task)
    VoiceAssistant::VoiceCommand response;
    if (!assistant.getLastResponse(response, REQUEST_TIMEOUT_MS)) {
        LOG_E("No response from VoiceAssistant for request %s", request.request_id.c_str());

        std::lock_guard<std::mutex> lock(results_mutex_);
        auto it = results_.find(request.request_id);
        if (it != results_.end()) {
            it->second.status = RequestStatus::FAILED;
            it->second.error_message = "No response from voice assistant";
            it->second.completed_at_ms = getCurrentTimeMs();
        }
        return;
    }

    // Success! Store the result
    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        auto it = results_.find(request.request_id);
        if (it != results_.end()) {
            it->second.status = RequestStatus::COMPLETED;
            it->second.response = response;
            it->second.completed_at_ms = getCurrentTimeMs();

            LOG_I("Request %s completed successfully", request.request_id.c_str());
        }
    }
}

std::string AsyncRequestManager::generateRequestId() {
    uint32_t counter = request_counter_.fetch_add(1);
    uint64_t timestamp = getCurrentTimeMs();

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "req_%llu_%u", timestamp, counter);
    return std::string(buffer);
}

uint64_t AsyncRequestManager::getCurrentTimeMs() const {
    return esp_timer_get_time() / 1000;
}

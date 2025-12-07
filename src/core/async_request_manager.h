#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <memory>

#include "core/voice_assistant.h"

/**
 * Async Request Manager
 * Manages asynchronous LLM requests without blocking the web server
 */
class AsyncRequestManager {
public:
    enum class RequestStatus {
        PENDING,    // Request queued, waiting to be processed
        PROCESSING, // Currently being processed by LLM
        COMPLETED,  // Successfully completed
        FAILED,     // Failed with error
        TIMEOUT     // Request timed out
    };

    struct RequestResult {
        RequestStatus status;
        VoiceAssistant::VoiceCommand response;
        std::string error_message;
        uint64_t created_at_ms;
        uint64_t completed_at_ms;
    };

    static AsyncRequestManager& getInstance();

    bool begin();
    void end();
    bool isRunning() const { return running_; }

    /**
     * Submit a new async request
     * @param message User message to send to LLM
     * @param request_id Output parameter for the generated request ID
     * @return true if request was queued successfully
     */
    bool submitRequest(const std::string& message, std::string& request_id);

    /**
     * Check the status of a request
     * @param request_id The request ID to check
     * @param result Output parameter for the request result
     * @return true if request_id exists, false otherwise
     */
    bool getRequestStatus(const std::string& request_id, RequestResult& result);

    /**
     * Cancel a pending or processing request
     * @param request_id The request ID to cancel
     * @return true if cancelled successfully
     */
    bool cancelRequest(const std::string& request_id);

    /**
     * Get number of pending requests
     */
    size_t getPendingCount() const;

    /**
     * Get number of processing requests
     */
    size_t getProcessingCount() const;

private:
    AsyncRequestManager();
    ~AsyncRequestManager();
    AsyncRequestManager(const AsyncRequestManager&) = delete;
    AsyncRequestManager& operator=(const AsyncRequestManager&) = delete;

    struct PendingRequest {
        std::string request_id;
        std::string message;
        uint64_t created_at_ms;
    };

    // Worker task that processes requests asynchronously
    static void workerTask(void* param);

    // Cleanup task that removes expired completed requests
    static void cleanupTask(void* param);

    // Generate unique request ID
    std::string generateRequestId();

    // Get current timestamp in milliseconds
    uint64_t getCurrentTimeMs() const;

    // Internal processing
    void processRequest(const PendingRequest& request);

    // Storage for pending and completed requests
    std::unordered_map<std::string, RequestResult> results_;
    mutable std::mutex results_mutex_;

    // Queue for pending requests
    QueueHandle_t request_queue_;

    // Worker task handle
    TaskHandle_t worker_task_;
    TaskHandle_t cleanup_task_;

    std::atomic<bool> running_{false};
    std::atomic<uint32_t> request_counter_{0};

    // Configuration
    static constexpr size_t MAX_PENDING_REQUESTS = 10;
    static constexpr size_t MAX_STORED_RESULTS = 50;
    static constexpr uint32_t CLEANUP_INTERVAL_MS = 30000;  // 30 seconds
    static constexpr uint32_t COMPLETED_REQUEST_TTL_MS = 300000;  // 5 minutes
    static constexpr uint32_t REQUEST_TIMEOUT_MS = 180000;  // 3 minutes max per request
};

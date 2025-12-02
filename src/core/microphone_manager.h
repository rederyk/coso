#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>
#include <string>
#include <functional>
#include <cstdint>

/**
 * @brief Centralized Microphone Manager
 *
 * Manages all microphone operations including:
 * - I2S exclusive management for recording
 * - WAV file generation
 * - Auto Gain Control (AGC)
 * - Coordination with AudioManager for I2S arbitration
 *
 * This class eliminates the need for UI screens (like MicrophoneTestScreen)
 * to contain business logic for audio recording.
 */
class MicrophoneManager {
public:
    /**
     * @brief Recording result structure
     */
    struct RecordingResult {
        bool success = false;
        std::string file_path;      // Absolute path to the recorded WAV file
        size_t file_size_bytes = 0;
        uint32_t duration_ms = 0;
        uint32_t sample_rate = 16000;
    };

    /**
     * @brief Recording configuration
     */
    struct RecordingConfig {
        uint32_t duration_seconds = 0;  // 0 = unlimited (controlled by stop_flag)
        uint32_t sample_rate = 16000;   // 16kHz default for voice
        uint8_t bits_per_sample = 16;
        uint8_t channels = 1;           // Mono
        bool enable_agc = true;         // Auto Gain Control enabled by default
        std::function<void(uint16_t)> level_callback = nullptr; // Real-time level updates (0-100%)
        const char* custom_directory = nullptr; // Optional custom directory (default: /test_recordings)
    };

    /**
     * @brief Recording handle (opaque pointer for managing recordings)
     */
    using RecordingHandle = void*;

    /**
     * @brief Get singleton instance
     */
    static MicrophoneManager& getInstance();

    /**
     * @brief Initialize the microphone manager
     * @return true if initialization succeeded
     */
    bool begin();

    /**
     * @brief Deinitialize and cleanup
     */
    void end();

    /**
     * @brief Start a new recording session
     *
     * This function creates a dedicated task for recording and returns immediately.
     * The recording runs asynchronously until either:
     * - The duration expires (if duration_seconds > 0)
     * - The stop_flag is set to true
     *
     * @param config Recording configuration
     * @param stop_flag Reference to atomic flag for early stop
     * @return RecordingHandle to retrieve results later
     */
    RecordingHandle startRecording(const RecordingConfig& config, std::atomic<bool>& stop_flag);

    /**
     * @brief Get the result of a recording (blocking until complete)
     *
     * This function waits for the recording task to complete and returns the result.
     *
     * @param handle The recording handle from startRecording()
     * @return RecordingResult with success status and file information
     */
    RecordingResult getRecordingResult(RecordingHandle handle);

    /**
     * @brief Check if a recording is currently in progress
     * @return true if recording is active
     */
    bool isRecording() const { return is_recording_.load(); }

    /**
     * @brief Enable or disable the microphone
     * @param enabled true to enable, false to disable
     * @return true if operation succeeded
     */
    bool setMicrophoneEnabled(bool enabled);

    /**
     * @brief Get the current microphone level
     * @return Current level (0-100%)
     */
    uint16_t getCurrentLevel() const { return current_level_.load(); }

private:
    MicrophoneManager();
    ~MicrophoneManager();
    MicrophoneManager(const MicrophoneManager&) = delete;
    MicrophoneManager& operator=(const MicrophoneManager&) = delete;

    /**
     * @brief Internal recording task context
     */
    struct RecordingContext {
        MicrophoneManager* manager;
        RecordingConfig config;
        std::atomic<bool>* stop_flag;
        RecordingResult result;
        TaskHandle_t task_handle = nullptr;
        bool completed = false;
    };

    /**
     * @brief Internal recording task implementation
     */
    static void recordingTaskImpl(void* param);

    /**
     * @brief Request exclusive I2S access (stops AudioManager playback)
     * @return true if I2S is available for recording
     */
    bool requestI2SExclusiveAccess();

    /**
     * @brief Release I2S exclusive access
     */
    void releaseI2SExclusiveAccess();

    // Internal state
    SemaphoreHandle_t i2s_mutex_ = nullptr;
    std::atomic<bool> is_recording_{false};
    std::atomic<uint16_t> current_level_{0};
    bool initialized_ = false;
};

#include "microphone_manager.h"
#include "audio_manager.h"
#include "utils/logger.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <algorithm>
#include <limits>
#include <cstring>

namespace {

// I2S Pin configuration (same as MicrophoneTestScreen)
constexpr int kMicI2sBckPin = 5;
constexpr int kMicI2sWsPin = 7;
constexpr int kMicI2sDinPin = 6;
constexpr int kMicI2sMckPin = 4;

// Default recording directory
constexpr const char* kRecordingsDir = "/test_recordings";

// AGC (Auto Gain Control) parameters
constexpr float kTargetPeak = 32000.0f;
constexpr float kMaxGainFactor = 20.0f;

// Recording buffer configuration
constexpr size_t kSamplesPerChunk = 2048;

/**
 * @brief WAV file header structure (PCM format)
 */
struct WAVHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t formatLength = 16;
    uint16_t formatType = 1; // PCM
    uint16_t channels = 1;
    uint32_t sampleRate = 16000;
    uint32_t bytesPerSecond = 32000; // sampleRate * channels * bitsPerSample / 8
    uint16_t blockAlign = 2; // channels * bitsPerSample / 8
    uint16_t bitsPerSample = 16;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize = 0;
};

/**
 * @brief Recording storage information
 */
enum class RecordingStorage {
    SD_CARD,
    LITTLEFS
};

struct RecordingStorageInfo {
    RecordingStorage storage = RecordingStorage::LITTLEFS;
    fs::FS* fs = nullptr;
    std::string directory = kRecordingsDir;
    const char* playback_prefix = "";
    const char* label = "LittleFS";
};

/**
 * @brief Determine available storage for recordings
 */
RecordingStorageInfo getRecordingStorageInfo(const char* custom_directory = nullptr) {
    RecordingStorageInfo info;
    if (SD_MMC.cardType() != CARD_NONE) {
        info.storage = RecordingStorage::SD_CARD;
        info.fs = &SD_MMC;
        info.playback_prefix = "/sd";
        info.label = "SD card";
    } else {
        info.storage = RecordingStorage::LITTLEFS;
        info.fs = &LittleFS;
        info.playback_prefix = "";
        info.label = "LittleFS";
    }
    // Use custom directory if provided
    if (custom_directory && custom_directory[0] != '\0') {
        info.directory = custom_directory;
    }
    return info;
}

/**
 * @brief Ensure recording directory exists
 */
bool ensureRecordingDirectory(const RecordingStorageInfo& info) {
    if (!info.fs) {
        Logger::getInstance().error("[MicMgr] No filesystem available for recordings");
        return false;
    }

    if (info.fs->exists(info.directory.c_str())) {
        return true;
    }

    if (!info.fs->mkdir(info.directory.c_str())) {
        Logger::getInstance().errorf("[MicMgr] Failed to create %s on %s", info.directory.c_str(), info.label);
        return false;
    }
    return true;
}

/**
 * @brief Parse recording index from filename (e.g., "test_000042.wav" -> 42)
 */
bool parseRecordingIndex(const std::string& path, uint32_t& out_index) {
    if (path.empty()) {
        return false;
    }

    size_t slash = path.find_last_of('/');
    size_t name_start = (slash == std::string::npos) ? 0 : slash + 1;
    constexpr const char* kPrefix = "test_";
    constexpr const char* kSuffix = ".wav";

    if (path.size() < name_start + strlen(kPrefix) + strlen(kSuffix)) {
        return false;
    }

    if (path.compare(name_start, strlen(kPrefix), kPrefix) != 0) {
        return false;
    }

    size_t suffix_pos = path.rfind(kSuffix);
    if (suffix_pos == std::string::npos || suffix_pos <= name_start + strlen(kPrefix)) {
        return false;
    }

    std::string number_str = path.substr(name_start + strlen(kPrefix),
                                         suffix_pos - (name_start + strlen(kPrefix)));
    if (number_str.empty()) {
        return false;
    }

    char* end_ptr = nullptr;
    long value = strtol(number_str.c_str(), &end_ptr, 10);
    if (!end_ptr || *end_ptr != '\0' || value < 0) {
        return false;
    }

    out_index = static_cast<uint32_t>(value);
    return true;
}

/**
 * @brief Find the next available recording index
 */
uint32_t findNextRecordingIndex(const RecordingStorageInfo& storage) {
    if (!storage.fs || !ensureRecordingDirectory(storage)) {
        return 0;
    }

    uint32_t max_index = 0;
    bool found_any = false;

    File dir = storage.fs->open(storage.directory.c_str());
    if (dir && dir.isDirectory()) {
        File entry = dir.openNextFile();
        while (entry) {
            if (!entry.isDirectory()) {
                uint32_t idx = 0;
                if (parseRecordingIndex(entry.name(), idx)) {
                    if (!found_any || idx > max_index) {
                        max_index = idx;
                        found_any = true;
                    }
                }
            }
            entry = dir.openNextFile();
        }
        dir.close();
    }

    if (!found_any) {
        return 0;
    }
    return max_index + 1;
}

/**
 * @brief Generate a unique filename for a new recording
 */
std::string generateRecordingFilename(const RecordingStorageInfo& storage) {
    uint32_t next_index = findNextRecordingIndex(storage);
    std::string directory = storage.directory;
    if (!directory.empty() && directory.back() == '/') {
        directory.pop_back();
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "%s/test_%06u.wav", directory.c_str(), next_index);
    return filename;
}

/**
 * @brief Build playback path with correct prefix for AudioManager
 */
std::string buildPlaybackPath(const RecordingStorageInfo& info, const std::string& relative_path) {
    if (info.playback_prefix && info.playback_prefix[0] != '\0') {
        return std::string(info.playback_prefix) + relative_path;
    }
    return relative_path;
}

} // namespace

// Singleton instance
MicrophoneManager& MicrophoneManager::getInstance() {
    static MicrophoneManager instance;
    return instance;
}

MicrophoneManager::MicrophoneManager() {
    i2s_mutex_ = xSemaphoreCreateMutex();
}

MicrophoneManager::~MicrophoneManager() {
    end();
    if (i2s_mutex_) {
        vSemaphoreDelete(i2s_mutex_);
        i2s_mutex_ = nullptr;
    }
}

bool MicrophoneManager::begin() {
    if (initialized_) {
        Logger::getInstance().warn("[MicMgr] Already initialized");
        return true;
    }

    Logger::getInstance().info("[MicMgr] Initializing microphone manager");

    if (!i2s_mutex_) {
        Logger::getInstance().error("[MicMgr] Failed to create I2S mutex");
        return false;
    }

    initialized_ = true;
    Logger::getInstance().info("[MicMgr] Microphone manager initialized");
    return true;
}

void MicrophoneManager::end() {
    if (!initialized_) {
        return;
    }

    Logger::getInstance().info("[MicMgr] Deinitializing microphone manager");
    initialized_ = false;
}

MicrophoneManager::RecordingHandle MicrophoneManager::startRecording(
    const RecordingConfig& config,
    std::atomic<bool>& stop_flag) {

    if (!initialized_) {
        Logger::getInstance().error("[MicMgr] Not initialized");
        return nullptr;
    }

    if (is_recording_.load()) {
        Logger::getInstance().warn("[MicMgr] Recording already in progress");
        return nullptr;
    }

    // Create recording context
    auto* ctx = new RecordingContext();
    ctx->manager = this;
    ctx->config = config;
    ctx->stop_flag = &stop_flag;
    ctx->completed = false;

    // Mark as recording
    is_recording_.store(true);

    // Create recording task
    xTaskCreate(
        recordingTaskImpl,
        "mic_recording",
        4096,
        ctx,
        4,  // High priority
        &ctx->task_handle
    );

    Logger::getInstance().info("[MicMgr] Recording task started");
    return ctx;
}

MicrophoneManager::RecordingResult MicrophoneManager::getRecordingResult(RecordingHandle handle) {
    if (!handle) {
        RecordingResult result;
        result.success = false;
        return result;
    }

    auto* ctx = static_cast<RecordingContext*>(handle);

    // Wait for recording task to complete
    while (!ctx->completed) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    RecordingResult result = ctx->result;

    // Cleanup context
    delete ctx;

    return result;
}

bool MicrophoneManager::setMicrophoneEnabled(bool enabled) {
    // TODO: Implement microphone enable/disable via codec control
    Logger::getInstance().infof("[MicMgr] Microphone %s", enabled ? "enabled" : "disabled");
    return true;
}

bool MicrophoneManager::requestI2SExclusiveAccess() {
    Logger::getInstance().info("[MicMgr] Requesting I2S exclusive access");

    // Stop AudioManager playback to free I2S_NUM_1
    AudioManager::getInstance().stop();

    // Small delay to ensure I2S is fully released
    vTaskDelay(pdMS_TO_TICKS(100));

    // Acquire mutex
    if (!i2s_mutex_) {
        Logger::getInstance().error("[MicMgr] I2S mutex not available");
        return false;
    }

    if (xSemaphoreTake(i2s_mutex_, pdMS_TO_TICKS(500)) != pdTRUE) {
        Logger::getInstance().error("[MicMgr] Failed to acquire I2S mutex");
        return false;
    }

    Logger::getInstance().info("[MicMgr] I2S exclusive access granted");
    return true;
}

void MicrophoneManager::releaseI2SExclusiveAccess() {
    if (i2s_mutex_) {
        xSemaphoreGive(i2s_mutex_);
        Logger::getInstance().info("[MicMgr] I2S exclusive access released");
    }
}

void MicrophoneManager::recordingTaskImpl(void* param) {
    auto* ctx = static_cast<RecordingContext*>(param);
    if (!ctx || !ctx->manager) {
        vTaskDelete(nullptr);
        return;
    }

    MicrophoneManager* manager = ctx->manager;
    RecordingConfig& config = ctx->config;
    std::atomic<bool>* stop_flag = ctx->stop_flag;

    Logger::getInstance().info("[MicMgr] Recording task implementation started");

    // Request I2S exclusive access
    if (!manager->requestI2SExclusiveAccess()) {
        Logger::getInstance().error("[MicMgr] Failed to acquire I2S access");
        ctx->result.success = false;
        ctx->completed = true;
        manager->is_recording_.store(false);
        vTaskDelete(nullptr);
        return;
    }

    // Get storage info and generate filename
    auto storage = getRecordingStorageInfo(config.custom_directory);
    if (!storage.fs || !ensureRecordingDirectory(storage)) {
        Logger::getInstance().error("[MicMgr] Unable to access recording storage");
        ctx->result.success = false;
        ctx->completed = true;
        manager->is_recording_.store(false);
        manager->releaseI2SExclusiveAccess();
        vTaskDelete(nullptr);
        return;
    }

    std::string filename = generateRecordingFilename(storage);
    Logger::getInstance().infof("[MicMgr] Recording to %s:%s", storage.label, filename.c_str());

    // Open file for writing
    File file = storage.fs->open(filename.c_str(), FILE_WRITE);
    if (!file) {
        Logger::getInstance().errorf("[MicMgr] Failed to open file on %s", storage.label);
        ctx->result.success = false;
        ctx->completed = true;
        manager->is_recording_.store(false);
        manager->releaseI2SExclusiveAccess();
        vTaskDelete(nullptr);
        return;
    }

    // Write initial WAV header (will be updated at the end)
    WAVHeader initial_header;
    initial_header.sampleRate = config.sample_rate;
    initial_header.channels = config.channels;
    initial_header.bitsPerSample = config.bits_per_sample;
    initial_header.bytesPerSecond = config.sample_rate * config.channels * config.bits_per_sample / 8;
    initial_header.blockAlign = config.channels * (config.bits_per_sample / 8);
    file.write((uint8_t*)&initial_header, sizeof(WAVHeader));

    // Configure I2S for microphone input
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = config.sample_rate,
        .bits_per_sample = i2s_bits_per_sample_t(config.bits_per_sample),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = true
    };

    const i2s_pin_config_t pin_config = {
        .mck_io_num = kMicI2sMckPin,
        .bck_io_num = kMicI2sBckPin,
        .ws_io_num = kMicI2sWsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = kMicI2sDinPin
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_1, &i2s_config, 0, nullptr);
    if (err != ESP_OK) {
        Logger::getInstance().errorf("[MicMgr] I2S install failed: %d", err);
        file.close();
        ctx->result.success = false;
        ctx->completed = true;
        manager->is_recording_.store(false);
        manager->releaseI2SExclusiveAccess();
        vTaskDelete(nullptr);
        return;
    }

    err = i2s_set_pin(I2S_NUM_1, &pin_config);
    if (err != ESP_OK) {
        Logger::getInstance().errorf("[MicMgr] I2S set pin failed: %d", err);
        i2s_driver_uninstall(I2S_NUM_1);
        file.close();
        ctx->result.success = false;
        ctx->completed = true;
        manager->is_recording_.store(false);
        manager->releaseI2SExclusiveAccess();
        vTaskDelete(nullptr);
        return;
    }

    i2s_set_clk(I2S_NUM_1, i2s_config.sample_rate, i2s_config.bits_per_sample, I2S_CHANNEL_MONO);

    // Allocate recording buffer
    uint8_t* buffer = static_cast<uint8_t*>(malloc(kSamplesPerChunk * sizeof(int16_t)));
    if (!buffer) {
        Logger::getInstance().error("[MicMgr] Failed to allocate audio buffer");
        i2s_driver_uninstall(I2S_NUM_1);
        file.close();
        ctx->result.success = false;
        ctx->completed = true;
        manager->is_recording_.store(false);
        manager->releaseI2SExclusiveAccess();
        vTaskDelete(nullptr);
        return;
    }

    const uint32_t target_duration_ms = config.duration_seconds * 1000;
    const bool limit_by_duration = config.duration_seconds > 0;
    uint32_t start_ms = millis();
    uint32_t total_bytes = 0;
    uint64_t recorded_samples = 0;

    Logger::getInstance().info("[MicMgr] Recording started");

    // Recording loop
    while (true) {
        if (stop_flag && stop_flag->load()) {
            Logger::getInstance().info("[MicMgr] Recording stop requested");
            break;
        }

        uint32_t elapsed_ms = millis() - start_ms;
        if (limit_by_duration && elapsed_ms >= target_duration_ms) {
            Logger::getInstance().info("[MicMgr] Recording duration reached");
            break;
        }

        size_t bytes_read = 0;
        err = i2s_read(I2S_NUM_1, buffer, kSamplesPerChunk * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(100));

        if (err == ESP_OK && bytes_read >= sizeof(int16_t)) {
            int16_t* samples = reinterpret_cast<int16_t*>(buffer);
            size_t sample_count = bytes_read / sizeof(int16_t);

            // Calculate peak level for this chunk
            int32_t chunk_peak = 0;
            for (size_t i = 0; i < sample_count; ++i) {
                int32_t value = std::abs(samples[i]);
                if (value > chunk_peak) {
                    chunk_peak = value;
                }
            }

            // Apply AGC if enabled
            float gain = 1.0f;
            int32_t scaled_peak = chunk_peak;

            if (config.enable_agc && chunk_peak > 0 && chunk_peak < kTargetPeak) {
                gain = std::min<float>(kMaxGainFactor, kTargetPeak / static_cast<float>(chunk_peak));
            }

            if (gain != 1.0f) {
                scaled_peak = 0;
                constexpr int32_t kInt16Min = std::numeric_limits<int16_t>::min();
                constexpr int32_t kInt16Max = std::numeric_limits<int16_t>::max();
                for (size_t i = 0; i < sample_count; ++i) {
                    float scaled_value = samples[i] * gain;
                    int32_t scaled_int = static_cast<int32_t>(scaled_value);
                    if (scaled_int < kInt16Min) {
                        scaled_int = kInt16Min;
                    } else if (scaled_int > kInt16Max) {
                        scaled_int = kInt16Max;
                    }
                    samples[i] = static_cast<int16_t>(scaled_int);
                    int32_t abs_value = std::abs(scaled_int);
                    if (abs_value > scaled_peak) {
                        scaled_peak = abs_value;
                    }
                }
            }

            // Write to file
            size_t mono_bytes = sample_count * sizeof(int16_t);
            file.write(reinterpret_cast<uint8_t*>(samples), mono_bytes);
            recorded_samples += sample_count;
            total_bytes += mono_bytes;

            // Update current level and call callback
            uint16_t level = static_cast<uint16_t>(std::min<int>(100, (scaled_peak * 100) / 32767));
            manager->current_level_.store(level);

            if (config.level_callback) {
                config.level_callback(level);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    // Reset level
    manager->current_level_.store(0);
    if (config.level_callback) {
        config.level_callback(0);
    }

    // Cleanup I2S
    free(buffer);
    i2s_driver_uninstall(I2S_NUM_1);

    uint32_t recording_duration_ms = millis() - start_ms;
    if (recording_duration_ms == 0) {
        recording_duration_ms = 1;
    }

    Logger::getInstance().infof("[MicMgr] Recording complete: %u bytes, %llu samples in %u ms",
                                total_bytes,
                                static_cast<unsigned long long>(recorded_samples),
                                recording_duration_ms);

    // Update WAV header with actual data size
    if (total_bytes > 0) {
        WAVHeader header;
        header.sampleRate = config.sample_rate;
        header.channels = config.channels;
        header.bitsPerSample = config.bits_per_sample;
        header.bytesPerSecond = config.sample_rate * config.channels * config.bits_per_sample / 8;
        header.blockAlign = config.channels * (config.bits_per_sample / 8);
        header.dataSize = total_bytes;
        header.fileSize = 36 + total_bytes; // 44 - 8 = 36

        file.seek(0);
        file.write((uint8_t*)&header, sizeof(WAVHeader));
    }

    file.close();

    // Release I2S access
    manager->releaseI2SExclusiveAccess();

    // Populate result
    ctx->result.success = (total_bytes > 0);
    ctx->result.file_path = buildPlaybackPath(storage, filename);
    ctx->result.duration_ms = recording_duration_ms;
    ctx->result.sample_rate = config.sample_rate;

    if (ctx->result.success) {
        // Get file size
        File check_file = storage.fs->open(filename.c_str(), FILE_READ);
        if (check_file) {
            ctx->result.file_size_bytes = check_file.size();
            check_file.close();
        }

        Logger::getInstance().infof("[MicMgr] Recording saved: %s (%u bytes)",
                                    ctx->result.file_path.c_str(),
                                    ctx->result.file_size_bytes);
    } else {
        Logger::getInstance().error("[MicMgr] Recording failed");
    }

    // Mark as completed
    ctx->completed = true;
    manager->is_recording_.store(false);

    Logger::getInstance().info("[MicMgr] Recording task ended");
    vTaskDelete(nullptr);
}

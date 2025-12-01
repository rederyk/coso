#include "core/voice_assistant.h"
#include "core/settings_manager.h"
#include "core/audio_manager.h"
#include "screens/microphone_test_screen.h"
#include "utils/logger.h"
#include <esp_http_client.h>
#include <cstring>
#include <WiFi.h>
#include <cJSON.h>

namespace {
constexpr const char* TAG = "VoiceAssistant";

#define LOG_I(format, ...) Logger::getInstance().infof("[VoiceAssistant] " format, ##__VA_ARGS__)
#define LOG_E(format, ...) Logger::getInstance().errorf("[VoiceAssistant] " format, ##__VA_ARGS__)
#define LOG_W(format, ...) Logger::getInstance().warnf("[VoiceAssistant] " format, ##__VA_ARGS__)

// Task priorities and stack sizes
constexpr UBaseType_t RECORDING_TASK_PRIORITY = 4;
constexpr size_t RECORDING_TASK_STACK = 4096;
constexpr UBaseType_t STT_TASK_PRIORITY = 3;
constexpr size_t STT_TASK_STACK = 8192;
constexpr UBaseType_t AI_TASK_PRIORITY = 3;
constexpr size_t AI_TASK_STACK = 8192;

// Queue sizes
constexpr size_t AUDIO_QUEUE_SIZE = 5;
constexpr size_t TEXT_QUEUE_SIZE = 5;
constexpr size_t COMMAND_QUEUE_SIZE = 10;
constexpr size_t VOICE_COMMAND_QUEUE_SIZE = 20;

// API endpoints
constexpr const char* WHISPER_ENDPOINT = "https://api.openai.com/v1/audio/transcriptions";
constexpr const char* GPT_ENDPOINT = "https://api.openai.com/v1/chat/completions";

// Recording config - unlimited duration (controlled by button press/release)
constexpr uint32_t RECORDING_DURATION_SECONDS = 0;

} // namespace

VoiceAssistant& VoiceAssistant::getInstance() {
    static VoiceAssistant instance;
    return instance;
}

VoiceAssistant::VoiceAssistant() {
    // Constructor - all initialization happens in begin()
    LOG_I("VoiceAssistant instance created");
}

VoiceAssistant::~VoiceAssistant() {
    // Cleanup happens in end()
}

bool VoiceAssistant::begin() {
    if (initialized_) {
        LOG_W("Already initialized");
        return true;
    }

    // Check if enabled in settings
    if (!isEnabled()) {
        LOG_I("Voice assistant disabled in settings");
        return false;
    }

    // Create queues
    audioQueue_ = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(QueueMessage*));
    transcriptionQueue_ = xQueueCreate(TEXT_QUEUE_SIZE, sizeof(std::string*));
    commandQueue_ = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(VoiceCommand*));
    voiceCommandQueue_ = xQueueCreate(VOICE_COMMAND_QUEUE_SIZE, sizeof(VoiceCommand*));

    if (!audioQueue_ || !transcriptionQueue_ || !commandQueue_ || !voiceCommandQueue_) {
        LOG_E("Failed to create queues");
        end();
        return false;
    }

    // Create tasks
    // Note: Recording task is created on-demand when startRecording() is called

    xTaskCreatePinnedToCore(
        speechToTextTask, "speech_to_text", STT_TASK_STACK,
        this, STT_TASK_PRIORITY, &sttTask_, tskNO_AFFINITY);

    xTaskCreatePinnedToCore(
        aiProcessingTask, "ai_processing", AI_TASK_STACK,
        this, AI_TASK_PRIORITY, &aiTask_, tskNO_AFFINITY);

    initialized_ = true;
    LOG_I("Voice assistant initialized successfully (recording delegated to MicrophoneTestScreen)");
    return true;
}

void VoiceAssistant::end() {
    initialized_ = false;

    // Stop any ongoing recording
    stop_recording_flag_.store(true);

    // Delete tasks
    if (recordingTask_) {
        vTaskDelete(recordingTask_);
        recordingTask_ = nullptr;
    }
    if (sttTask_) {
        vTaskDelete(sttTask_);
        sttTask_ = nullptr;
    }
    if (aiTask_) {
        vTaskDelete(aiTask_);
        aiTask_ = nullptr;
    }

    // Delete queues
    if (audioQueue_) {
        vQueueDelete(audioQueue_);
        audioQueue_ = nullptr;
    }
    if (transcriptionQueue_) {
        vQueueDelete(transcriptionQueue_);
        transcriptionQueue_ = nullptr;
    }
    if (commandQueue_) {
        vQueueDelete(commandQueue_);
        commandQueue_ = nullptr;
    }
    if (voiceCommandQueue_) {
        vQueueDelete(voiceCommandQueue_);
        voiceCommandQueue_ = nullptr;
    }

    LOG_I("Voice assistant deinitialized");
}

bool VoiceAssistant::isEnabled() const {
    return SettingsManager::getInstance().getVoiceAssistantEnabled();
}

void VoiceAssistant::triggerListening() {
    LOG_I("Voice assistant manually triggered (bypass wake word)");
    // TODO: Signal the capture task to start listening
    // For now, just log the trigger
}

void VoiceAssistant::startRecording() {
    LOG_I("Starting voice recording session (delegating to MicrophoneTestScreen)");

    if (recordingTask_) {
        LOG_W("Recording already in progress");
        return;
    }

    // Reset stop flag
    stop_recording_flag_.store(false);
    last_recorded_file_.clear();

    // Create recording task
    xTaskCreatePinnedToCore(
        recordingTask, "voice_recording", RECORDING_TASK_STACK,
        this, RECORDING_TASK_PRIORITY, &recordingTask_, tskNO_AFFINITY);
}

void VoiceAssistant::stopRecordingAndProcess() {
    LOG_I("Stopping voice recording and starting processing");

    // Signal the recording task to stop
    stop_recording_flag_.store(true);

    // The recording task will handle cleanup and file creation
    // When the task completes, it will send the file to STT processing
}

// Task implementations
void VoiceAssistant::recordingTask(void* param) {
    VoiceAssistant* va = static_cast<VoiceAssistant*>(param);
    if (!va) {
        vTaskDelete(nullptr);
        return;
    }

    LOG_I("Recording task started - delegating to MicrophoneTestScreen");

    // CRITICAL: Stop any ongoing audio playback to free I2S_NUM_1
    // The recording function needs exclusive access to the I2S port
    AudioManager::getInstance().stop();
    LOG_I("Stopped audio playback to free I2S for recording");

    // Small delay to ensure I2S is fully released
    vTaskDelay(pdMS_TO_TICKS(100));

    // Call MicrophoneTestScreen's recording function
    auto result = MicrophoneTestScreen::recordToFile(
        RECORDING_DURATION_SECONDS,
        va->stop_recording_flag_
    );

    if (result.success) {
        LOG_I("Recording completed successfully: %s (%u bytes, %u ms)",
              result.file_path.c_str(),
              result.file_size_bytes,
              result.duration_ms);

        va->last_recorded_file_ = result.file_path;

        // TODO: Send the recorded file to STT processing
        // For now, just log that we have the file ready
        LOG_I("Audio file ready for STT processing: %s", va->last_recorded_file_.c_str());

        // In the future, we'll read this WAV file and send it to the STT task
        // via the audioQueue_

    } else {
        LOG_E("Recording failed");
    }

    // Clear task handle and cleanup
    va->recordingTask_ = nullptr;
    va->stop_recording_flag_.store(false);

    LOG_I("Recording task ended");
    vTaskDelete(nullptr);
}

void VoiceAssistant::speechToTextTask(void* param) {
    VoiceAssistant* va = static_cast<VoiceAssistant*>(param);
    LOG_I("Speech-to-text task started");

    // TODO: Implement STT processing
    while (va->initialized_) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    LOG_I("Speech-to-text task ended");
    vTaskDelete(NULL);
}

void VoiceAssistant::aiProcessingTask(void* param) {
    VoiceAssistant* va = static_cast<VoiceAssistant*>(param);
    LOG_I("AI processing task started");

    // TODO: Implement AI processing
    while (va->initialized_) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    LOG_I("AI processing task ended");
    vTaskDelete(NULL);
}

// HTTP helpers (stubs for initial integration)
bool VoiceAssistant::makeWhisperRequest(const AudioBuffer* audio, std::string& transcription) {
    // TODO: Implement Whisper API call
    return false;
}

bool VoiceAssistant::makeGPTRequest(const std::string& prompt, std::string& response) {
    // TODO: Implement GPT API call
    return false;
}

bool VoiceAssistant::parseGPTCommand(const std::string& response, VoiceCommand& cmd) {
    // TODO: Implement JSON parsing with cJSON
    return false;
}

// Queue helpers
bool VoiceAssistant::sendAudioBuffer(AudioBuffer* buffer) {
    if (!audioQueue_) return false;

    QueueMessage* msg = new QueueMessage();
    msg->type = MessageType::AudioBuffer;
    msg->payload.audio_buffer = buffer;

    if (xQueueSend(audioQueue_, &msg, pdMS_TO_TICKS(100)) != pdPASS) {
        LOG_W("Audio queue full, discarding buffer");
        delete buffer; // Free buffer if queue is full
        delete msg;
        return false;
    }

    return true;
}

bool VoiceAssistant::receiveTranscribedText(std::string& text) {
    // TODO: Implement text receive
    return false;
}

bool VoiceAssistant::sendCommand(VoiceCommand* cmd) {
    // TODO: Implement command send
    return false;
}

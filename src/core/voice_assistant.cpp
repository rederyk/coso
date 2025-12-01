#include "core/voice_assistant.h"
#include "core/settings_manager.h"
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
constexpr UBaseType_t CAPTURE_TASK_PRIORITY = 4;
constexpr size_t CAPTURE_TASK_STACK = 6144;
constexpr UBaseType_t STT_TASK_PRIORITY = 3;
constexpr size_t STT_TASK_STACK = 8192;
constexpr UBaseType_t AI_TASK_PRIORITY = 3;
constexpr size_t AI_TASK_STACK = 8192;

// Queue sizes
constexpr size_t AUDIO_QUEUE_SIZE = 5;
constexpr size_t TEXT_QUEUE_SIZE = 5;
constexpr size_t COMMAND_QUEUE_SIZE = 10;
constexpr size_t VOICE_COMMAND_QUEUE_SIZE = 20;

// Audio config
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint8_t BITS_PER_SAMPLE = 16;
constexpr uint8_t CHANNELS = 1;

// API endpoints
constexpr const char* WHISPER_ENDPOINT = "https://api.openai.com/v1/audio/transcriptions";
constexpr const char* GPT_ENDPOINT = "https://api.openai.com/v1/chat/completions";

} // namespace

VoiceAssistant& VoiceAssistant::getInstance() {
    static VoiceAssistant instance;
    return instance;
}

VoiceAssistant::VoiceAssistant() {

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
    xTaskCreatePinnedToCore(
        voiceCaptureTask, "voice_capture", CAPTURE_TASK_STACK,
        this, CAPTURE_TASK_PRIORITY, &captureTask_, tskNO_AFFINITY);

    xTaskCreatePinnedToCore(
        speechToTextTask, "speech_to_text", STT_TASK_STACK,
        this, STT_TASK_PRIORITY, &sttTask_, tskNO_AFFINITY);

    xTaskCreatePinnedToCore(
        aiProcessingTask, "ai_processing", AI_TASK_STACK,
        this, AI_TASK_PRIORITY, &aiTask_, tskNO_AFFINITY);

    initialized_ = true;
    LOG_I("Voice assistant initialized successfully");
    return true;
}

void VoiceAssistant::end() {
    initialized_ = false;

    // Delete tasks
    if (captureTask_) {
        vTaskDelete(captureTask_);
        captureTask_ = nullptr;
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

// Task implementations (stubs for initial integration)
void VoiceAssistant::voiceCaptureTask(void* param) {
    VoiceAssistant* va = static_cast<VoiceAssistant*>(param);
    LOG_I("Voice capture task started");

    // TODO: Implement audio capture
    while (va->initialized_) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    LOG_I("Voice capture task ended");
    vTaskDelete(NULL);
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

// Queue helpers (stubs)
bool VoiceAssistant::sendAudioBuffer(AudioBuffer* buffer) {
    // TODO: Implement audio send
    return false;
}

bool VoiceAssistant::receiveTranscribedText(std::string& text) {
    // TODO: Implement text receive
    return false;
}

bool VoiceAssistant::sendCommand(VoiceCommand* cmd) {
    // TODO: Implement command send
    return false;
}

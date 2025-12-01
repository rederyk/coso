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
constexpr int MIC_I2S_BCK_PIN = 5;
constexpr int MIC_I2S_WS_PIN = 7;
constexpr int MIC_I2S_DATA_PIN = 6;

// API endpoints
constexpr const char* WHISPER_ENDPOINT = "https://api.openai.com/v1/audio/transcriptions";
constexpr const char* GPT_ENDPOINT = "https://api.openai.com/v1/chat/completions";

} // namespace

VoiceAssistant& VoiceAssistant::getInstance() {
    static VoiceAssistant instance;
    return instance;
}

VoiceAssistant::VoiceAssistant() {
    // Initialize audio buffer for input
    psram_buffer_ = static_cast<uint8_t*>(heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_SPIRAM));
    if (!psram_buffer_) {
        LOG_E("Failed to allocate PSRAM buffer for audio input");
    }
}

VoiceAssistant::~VoiceAssistant() {
    if (psram_buffer_) {
        heap_caps_free(psram_buffer_);
        psram_buffer_ = nullptr;
    }
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

void VoiceAssistant::triggerListening() {
    LOG_I("Voice assistant manually triggered (bypass wake word)");
    // TODO: Signal the capture task to start listening
    // For now, just log the trigger
}

// Task implementations
void VoiceAssistant::voiceCaptureTask(void* param) {
    VoiceAssistant* va = static_cast<VoiceAssistant*>(param);
    LOG_I("Voice capture task started");

    // Configure microphone if not already enabled
    // TODO: Enable microphone in ES8311 codec
    LOG_I("Microphone initialization pending - need codec access");

    // Initialize I2S for microphone input
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = i2s_bits_per_sample_t(BITS_PER_SAMPLE),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false
    };

    const i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = MIC_I2S_BCK_PIN,   // I2S BCLK pin (shared with playback)
        .ws_io_num = MIC_I2S_WS_PIN,    // I2S WS (LRCLK) pin
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_I2S_DATA_PIN   // I2S DIN pin for microphone
    };

    esp_err_t err = i2s_driver_install(va->i2s_input_port_, &i2s_config, 0, nullptr);
    if (err != ESP_OK) {
        LOG_E("Failed to install I2S driver: %d", err);
        return;
    }

    err = i2s_set_pin(va->i2s_input_port_, &pin_config);
    if (err != ESP_OK) {
        LOG_E("Failed to set I2S pins: %d", err);
        i2s_driver_uninstall(va->i2s_input_port_);
        return;
    }

    LOG_I("I2S input driver configured for microphone");

    // Simple VAD settings
    const int16_t VAD_THRESHOLD = 800;  // Volume threshold for voice detection
    const size_t CHUNK_SIZE = 1024;     // Samples to read per chunk (1024 * 2 bytes = 2KB)
    const size_t SILENCE_DURATION_MS = 2000; // Stop recording after 2s silence
    const TickType_t SILENCE_TIMEOUT = pdMS_TO_TICKS(SILENCE_DURATION_MS);
    const size_t MAX_RECORDING_SAMPLES = (SAMPLE_RATE * 10); // Max 10 seconds recording

    size_t total_samples = 0;
    bool is_recording = false;
    TickType_t last_voice_tick = xTaskGetTickCount();

    while (va->initialized_) {
        size_t bytes_read = 0;

        // Read audio chunk
        if (va->psram_buffer_) {
            err = i2s_read(va->i2s_input_port_, va->psram_buffer_, CHUNK_SIZE * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(100));
            if (err != ESP_OK) {
                LOG_W("I2S read error: %d", err);
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (bytes_read == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            size_t samples_read = bytes_read / sizeof(int16_t);
            total_samples += samples_read;

            // Simple VAD: check for samples above threshold
            bool has_voice = false;
            const int16_t* pcm_samples = reinterpret_cast<const int16_t*>(va->psram_buffer_);

            for (size_t i = 0; i < samples_read; ++i) {
                if (abs(pcm_samples[i]) > VAD_THRESHOLD) {
                    has_voice = true;
                    break;
                }
            }

            if (has_voice) {
                if (!is_recording) {
                    LOG_I("Voice detected, starting recording");
                    is_recording = true;
                    total_samples = 0; // Reset count for new recording
                }
                last_voice_tick = xTaskGetTickCount();

                // Send audio buffer if recording and queue not full
                if (is_recording && total_samples < MAX_RECORDING_SAMPLES) {
                    // Create AudioBuffer for downstream processing
                    AudioBuffer* buffer = new AudioBuffer();
                    buffer->data = new uint8_t[bytes_read];
                    memcpy(buffer->data, va->psram_buffer_, bytes_read);
                    buffer->size = bytes_read;
                    buffer->sample_rate = SAMPLE_RATE;
                    buffer->bits_per_sample = BITS_PER_SAMPLE;
                    buffer->channels = CHANNELS;

                    va->sendAudioBuffer(buffer);
                }
            } else if (is_recording) {
                // Check for silence timeout
                if (xTaskGetTickCount() - last_voice_tick > SILENCE_TIMEOUT) {
                    LOG_I("Silence timeout, stopping recording (recorded %d samples)", total_samples);
                    is_recording = false;
                    // Could send end-of-speech signal here
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent hogging CPU
    }

    // Cleanup
    i2s_driver_uninstall(va->i2s_input_port_);
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

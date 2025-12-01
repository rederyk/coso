#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <cJSON.h>
#include <string>
#include <vector>

/**
 * Voice Assistant Module
 * Implements local voice processing using OpenAI APIs with three FreeRTOS tasks
 */
class VoiceAssistant {
public:
    /** Command message structure */
    struct VoiceCommand {
        std::string command;
        std::vector<std::string> args;
        VoiceCommand() = default;
        VoiceCommand(std::string cmd, std::vector<std::string> a) : command(std::move(cmd)), args(std::move(a)) {}
    };

    /** Audio buffer structure */
    struct AudioBuffer {
        uint8_t* data = nullptr;
        size_t size = 0;
        uint32_t sample_rate = 16000;
        uint8_t bits_per_sample = 16;
        uint8_t channels = 1;
        ~AudioBuffer() { delete[] data; }
    };

    /** Queue message types */
    enum class MessageType {
        AudioBuffer,
        TranscribedText,
        Command,
        Error
    };

    /** Queue message */
    struct QueueMessage {
        MessageType type;
        union {
            AudioBuffer* audio_buffer;
            std::string* text_data;
            VoiceCommand* command;
            const char* error_msg;
        } payload;
    };

    static VoiceAssistant& getInstance();

    bool begin();
    void end();

    bool isEnabled() const;

    /** Manually trigger voice assistant listening, bypassing wake word */
    void triggerListening();

    QueueHandle_t getCommandQueue() const { return voiceCommandQueue_; }

private:
    VoiceAssistant();
    ~VoiceAssistant() = default;
    VoiceAssistant(const VoiceAssistant&) = delete;
    VoiceAssistant& operator=(const VoiceAssistant&) = delete;

    // Task functions
    static void voiceCaptureTask(void* param);
    static void speechToTextTask(void* param);
    static void aiProcessingTask(void* param);

    // HTTP helpers
    bool makeWhisperRequest(const AudioBuffer* audio, std::string& transcription);
    bool makeGPTRequest(const std::string& prompt, std::string& response);
    bool parseGPTCommand(const std::string& response, VoiceCommand& cmd);

    // Queue helpers
    bool sendAudioBuffer(AudioBuffer* buffer);
    bool receiveTranscribedText(std::string& text);
    bool sendCommand(VoiceCommand* cmd);

    QueueHandle_t audioQueue_ = nullptr;
    QueueHandle_t transcriptionQueue_ = nullptr;
    QueueHandle_t commandQueue_ = nullptr;
    QueueHandle_t voiceCommandQueue_ = nullptr; // Public command output

    TaskHandle_t captureTask_ = nullptr;
    TaskHandle_t sttTask_ = nullptr;
    TaskHandle_t aiTask_ = nullptr;

    bool initialized_ = false;
};

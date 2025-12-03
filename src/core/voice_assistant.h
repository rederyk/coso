#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <cJSON.h>
#include <string>
#include <vector>
#include <atomic>

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

    /** Start recording audio (called when button is pressed) */
    void startRecording();

    /** Stop recording and process the captured audio (called when button is released) */
    void stopRecordingAndProcess();

    QueueHandle_t getCommandQueue() const { return voiceCommandQueue_; }

    /** Get the last recorded file path (for external use) */
    std::string getLastRecordedFile() const { return last_recorded_file_; }

    /** Fetch available models from Ollama API */
    bool fetchOllamaModels(const std::string& base_url, std::vector<std::string>& models);

private:
    VoiceAssistant();
    ~VoiceAssistant();
    VoiceAssistant(const VoiceAssistant&) = delete;
    VoiceAssistant& operator=(const VoiceAssistant&) = delete;

    // Task functions
    static void recordingTask(void* param);      // Manages recording via MicrophoneTestScreen
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

    TaskHandle_t recordingTask_ = nullptr;    // Manages recording via MicrophoneTestScreen
    TaskHandle_t sttTask_ = nullptr;
    TaskHandle_t aiTask_ = nullptr;

    bool initialized_ = false;
    std::atomic<bool> stop_recording_flag_{false};  // Flag to stop recording
    std::string last_recorded_file_;                // Path to last recorded file
};

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <cJSON.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <queue>

#include "core/voice_assistant_prompt.h"
#include "core/command_center.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

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
        std::string text;           // Conversational response text from LLM
        std::string transcription;  // Original user speech transcription
        std::string output;         // Command execution output/result

        // Output refinement fields (Phase 1: Output Refinement System)
        std::string refined_output; // Post-processed user-friendly output
        bool needs_refinement;      // Flag indicating if output should be refined
        std::string refinement_extract_field; // Which field to extract from refinement JSON (Phase 2)

        VoiceCommand() : needs_refinement(false), refinement_extract_field("text") {}
        VoiceCommand(std::string cmd, std::vector<std::string> a, std::string txt = "", std::string speech = "", std::string out = "")
            : command(std::move(cmd)),
              args(std::move(a)),
              text(std::move(txt)),
              transcription(std::move(speech)),
              output(std::move(out)),
              needs_refinement(false),
              refinement_extract_field("text") {}
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
    bool isInitialized() const;

    /** Manually trigger voice assistant listening, bypassing wake word */
    void triggerListening();

    /** Start recording audio (called when button is pressed) */
    void startRecording();

    /** Stop recording and process the captured audio (called when button is released) */
    void stopRecordingAndProcess();

    /** Send text message directly to LLM (for chat interface, bypasses STT) */
    bool sendTextMessage(const std::string& text);

    /** Get the last LLM response (blocking call with timeout) */
    bool getLastResponse(VoiceCommand& response, uint32_t timeout_ms = 5000);

    QueueHandle_t getCommandQueue() const { return voiceCommandQueue_; }

    /** Get the last recorded file path (for external use) */
    std::string getLastRecordedFile() const;

    /** Build the current system prompt, including the active command list */
    std::string getSystemPrompt() const;

    /** Fetch available models from Ollama API */
    bool fetchOllamaModels(const std::string& base_url, std::vector<std::string>& models);

    /** Lua scripting sandbox for complex commands */
    class LuaSandbox {
    private:
        lua_State* L;

        void setupSandbox();
        std::string preprocessScript(const std::string& script);

    public:
        LuaSandbox();
        ~LuaSandbox();

        CommandResult execute(const std::string& script);

        // Lua C API bindings
        static int lua_gpio_write(lua_State* L);
        static int lua_gpio_read(lua_State* L);
        static int lua_gpio_toggle(lua_State* L);
        static int lua_delay(lua_State* L);
        static int lua_ble_type(lua_State* L);
        static int lua_ble_send_key(lua_State* L);
        static int lua_ble_mouse_move(lua_State* L);
        static int lua_ble_click(lua_State* L);
        static int lua_volume_up(lua_State* L);
        static int lua_volume_down(lua_State* L);
        static int lua_brightness_up(lua_State* L);
        static int lua_brightness_down(lua_State* L);
        static int lua_led_brightness(lua_State* L);
        static int lua_ping(lua_State* L);
        static int lua_uptime(lua_State* L);
        static int lua_heap(lua_State* L);
        static int lua_sd_status(lua_State* L);
        static int lua_system_status(lua_State* L);
        static int lua_webdata_fetch_once(lua_State* L);
        static int lua_webdata_fetch_scheduled(lua_State* L);
        static int lua_webdata_read_data(lua_State* L);
        static int lua_webdata_list_files(lua_State* L);
        static int lua_memory_read_file(lua_State* L);
        static int lua_memory_write_file(lua_State* L);
        static int lua_memory_list_files(lua_State* L);
        static int lua_memory_delete_file(lua_State* L);
        static int lua_println(lua_State* L);
        void appendOutput(const std::string& text);
        std::string output_buffer_;
    };

    CommandResult executeLuaScript(const std::string& script);

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
    bool makeWhisperRequest(const std::string& file_path, std::string& transcription);
    bool makeGPTRequest(const std::string& prompt, std::string& response);
    bool parseGPTCommand(const std::string& response, VoiceCommand& cmd);

    // Output refinement helpers (Phase 1: Output Refinement System)
    bool shouldRefineOutput(const VoiceCommand& cmd);
    bool refineCommandOutput(VoiceCommand& cmd);
    std::string buildRefinementPrompt(const VoiceCommand& cmd);

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
    mutable std::mutex last_recorded_mutex_;

    std::queue<std::string> pending_recordings_;
    std::mutex pending_recordings_mutex_;

    LuaSandbox lua_sandbox_;  // Lua scripting engine
    std::mutex lua_mutex_;
};

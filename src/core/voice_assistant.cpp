#include "core/voice_assistant.h"
#include "core/settings_manager.h"
#include "core/audio_manager.h"
#include "core/microphone_manager.h"
#include "core/command_center.h"
#include "utils/logger.h"
#include <esp_http_client.h>
#include <cstring>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
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

// Assistant recordings directory (separate from test recordings)
constexpr const char* ASSISTANT_RECORDINGS_DIR = "/assistant_recordings";

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
    LOG_I("Voice assistant initialized successfully (using MicrophoneManager)");
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
    LOG_I("Starting voice recording session (using MicrophoneManager)");

    if (recordingTask_) {
        LOG_W("Recording already in progress");
        return;
    }

    // Reset stop flag
    stop_recording_flag_.store(false);
    last_recorded_file_.clear();

    // Create recording task (lightweight wrapper around MicrophoneManager)
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

    LOG_I("Recording task started - using MicrophoneManager");

    // Configure recording (unlimited duration, controlled by stop flag)
    MicrophoneManager::RecordingConfig config;
    config.duration_seconds = RECORDING_DURATION_SECONDS;  // 0 = unlimited
    config.sample_rate = 16000;
    config.bits_per_sample = 16;
    config.channels = 1;
    config.enable_agc = true;
    config.level_callback = nullptr;  // No UI updates needed for voice assistant
    config.custom_directory = ASSISTANT_RECORDINGS_DIR;  // Use dedicated assistant recordings directory
    config.filename_prefix = "assistant";  // Use "assistant" prefix for filenames

    // Start recording using MicrophoneManager
    auto handle = MicrophoneManager::getInstance().startRecording(
        config,
        va->stop_recording_flag_
    );

    if (!handle) {
        LOG_E("Failed to start recording");
        va->recordingTask_ = nullptr;
        va->stop_recording_flag_.store(false);
        vTaskDelete(nullptr);
        return;
    }

    // Wait for recording to complete and get result
    auto result = MicrophoneManager::getInstance().getRecordingResult(handle);

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

    while (va->initialized_) {
        // Wait for audio file to be ready (signaled by recordingTask completing)
        // We use a simple polling approach: check if last_recorded_file_ is set
        if (!va->last_recorded_file_.empty()) {
            LOG_I("Processing audio file for STT: %s", va->last_recorded_file_.c_str());

            std::string transcription;
            bool success = va->makeWhisperRequest(nullptr, transcription);  // File path is in last_recorded_file_

            if (success && !transcription.empty()) {
                LOG_I("STT successful: %s", transcription.c_str());

                // Send transcription to AI processing task
                std::string* text_copy = new std::string(transcription);
                if (xQueueSend(va->transcriptionQueue_, &text_copy, pdMS_TO_TICKS(1000)) != pdPASS) {
                    LOG_W("Transcription queue full, discarding text");
                    delete text_copy;
                }
            } else {
                LOG_E("STT failed or empty transcription");
            }

            // Clear the last recorded file to avoid reprocessing
            va->last_recorded_file_.clear();
        }

        // Sleep to avoid busy-waiting
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    LOG_I("Speech-to-text task ended");
    vTaskDelete(NULL);
}

void VoiceAssistant::aiProcessingTask(void* param) {
    VoiceAssistant* va = static_cast<VoiceAssistant*>(param);
    LOG_I("AI processing task started");

    while (va->initialized_) {
        // Wait for transcribed text from STT task
        std::string* transcription = nullptr;
        if (xQueueReceive(va->transcriptionQueue_, &transcription, pdMS_TO_TICKS(1000)) == pdPASS) {
            if (transcription && !transcription->empty()) {
                LOG_I("Processing transcription with LLM: %s", transcription->c_str());

                // Call LLM to convert text to command
                std::string llm_response;
                bool success = va->makeGPTRequest(*transcription, llm_response);

                if (success && !llm_response.empty()) {
                    LOG_I("LLM response received");

                    // Parse command from LLM response
                    VoiceCommand cmd;
                    if (va->parseGPTCommand(llm_response, cmd)) {
                        LOG_I("Command parsed successfully: %s", cmd.command.c_str());

                        // Execute command via CommandCenter
                        CommandResult result = CommandCenter::getInstance().executeCommand(cmd.command, cmd.args);

                        if (result.success) {
                            LOG_I("Command executed successfully: %s", result.message.c_str());
                        } else {
                            LOG_E("Command execution failed: %s", result.message.c_str());
                        }

                        // Optionally: Send result to voice command queue for external consumption
                        VoiceCommand* cmd_copy = new VoiceCommand(cmd);
                        if (xQueueSend(va->voiceCommandQueue_, &cmd_copy, pdMS_TO_TICKS(100)) != pdPASS) {
                            LOG_W("Voice command queue full");
                            delete cmd_copy;
                        }
                    } else {
                        LOG_E("Failed to parse command from LLM response");
                    }
                } else {
                    LOG_E("LLM request failed or empty response");
                }

                delete transcription;
            }
        }
    }

    LOG_I("AI processing task ended");
    vTaskDelete(NULL);
}

// HTTP helpers - Whisper STT API
bool VoiceAssistant::makeWhisperRequest(const AudioBuffer* audio, std::string& transcription) {
    LOG_I("Making Whisper STT request (file-based implementation)");

    // Check if we have WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        LOG_E("WiFi not connected");
        return false;
    }

    // Get the file path from last_recorded_file_ (set by recordingTask)
    if (last_recorded_file_.empty()) {
        LOG_E("No recorded file available");
        return false;
    }

    LOG_I("Attempting to open file: '%s'", last_recorded_file_.c_str());
    LOG_I("File path length: %d bytes", last_recorded_file_.length());

    // MicrophoneManager returns paths with "/sd" prefix for SD card.
    // We need to use SD_MMC.open() with the path WITHOUT the "/sd" prefix.
    std::string file_path = last_recorded_file_;
    if (file_path.find("/sd/") == 0) {
        file_path = file_path.substr(3);  // Remove "/sd" prefix
        LOG_I("Removed /sd prefix, using path: '%s'", file_path.c_str());
    }

    // Open the WAV file using SD_MMC (Arduino File API)
    File file = SD_MMC.open(file_path.c_str(), FILE_READ);
    if (!file) {
        LOG_E("Failed to open audio file: %s", file_path.c_str());
        LOG_E("SD_MMC.open() returned null");
        return false;
    }

    LOG_I("File opened successfully");

    // Get file size
    size_t file_size = file.size();
    LOG_I("Audio file size: %u bytes", file_size);

    if (file_size == 0) {
        LOG_E("File is empty");
        file.close();
        return false;
    }

    // Allocate buffer for file data in PSRAM
    uint8_t* file_data = (uint8_t*)heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!file_data) {
        LOG_E("Failed to allocate memory for file data");
        file.close();
        return false;
    }

    // Read file into buffer
    size_t bytes_read = file.read(file_data, file_size);
    file.close();

    if (bytes_read != file_size) {
        LOG_E("Failed to read complete file (read %u of %u bytes)", bytes_read, file_size);
        heap_caps_free(file_data);
        return false;
    }

    LOG_I("File read successfully: %u bytes", bytes_read);

    // Get endpoint from settings - support both local and cloud modes
    std::string whisper_url;
    const SettingsSnapshot& settings = SettingsManager::getInstance().getSnapshot();

    if (settings.localApiMode) {
        // Local Docker mode: http://<docker_ip>:8000/v1/audio/transcriptions
        whisper_url = "http://" + settings.dockerHostIp + ":8000/v1/audio/transcriptions";
        LOG_I("Using LOCAL Whisper API at: %s", whisper_url.c_str());
    } else {
        // Cloud mode: use configured endpoint
        whisper_url = settings.openAiEndpoint + "/audio/transcriptions";
        LOG_I("Using CLOUD Whisper API at: %s", whisper_url.c_str());
    }

    // Build multipart/form-data request manually
    const char* boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    // Calculate content length for multipart body
    std::string header_part = std::string("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";

    std::string model_part = std::string("\r\n--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "whisper-1\r\n";  // For OpenAI compatibility, faster-whisper accepts any model name

    std::string footer_part = std::string("--") + boundary + "--\r\n";

    size_t total_length = header_part.length() + file_size + model_part.length() + footer_part.length();

    // Buffer for response
    std::string response_buffer;
    response_buffer.reserve(1024);

    // HTTP client event handler
    auto http_event_handler = [](esp_http_client_event_t *evt) -> esp_err_t {
        std::string* response = static_cast<std::string*>(evt->user_data);
        switch(evt->event_id) {
            case HTTP_EVENT_ON_DATA:
                if (response && evt->data_len > 0) {
                    response->append((char*)evt->data, evt->data_len);
                }
                break;
            default:
                break;
        }
        return ESP_OK;
    };

    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = whisper_url.c_str();
    config.method = HTTP_METHOD_POST;
    config.event_handler = http_event_handler;
    config.user_data = &response_buffer;
    config.timeout_ms = 30000;  // 30 second timeout
    config.buffer_size = 4096;
    config.buffer_size_tx = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        LOG_E("Failed to initialize HTTP client");
        heap_caps_free(file_data);
        return false;
    }

    // Set headers
    std::string content_type = std::string("multipart/form-data; boundary=") + boundary;
    esp_http_client_set_header(client, "Content-Type", content_type.c_str());

    // Optional: Add API key if using OpenAI cloud (local Whisper doesn't need it)
    if (!settings.localApiMode && !settings.openAiApiKey.empty()) {
        std::string auth_header = std::string("Bearer ") + settings.openAiApiKey;
        esp_http_client_set_header(client, "Authorization", auth_header.c_str());
        LOG_I("Using API key for cloud authentication");
    }

    // Open connection
    esp_err_t err = esp_http_client_open(client, total_length);
    if (err != ESP_OK) {
        LOG_E("Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        heap_caps_free(file_data);
        return false;
    }

    // Write multipart body in chunks
    int written = 0;

    // Write header part
    written = esp_http_client_write(client, header_part.c_str(), header_part.length());
    if (written < 0) {
        LOG_E("Failed to write header part");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        heap_caps_free(file_data);
        return false;
    }

    // Write file data in chunks
    const size_t chunk_size = 4096;
    size_t offset = 0;
    while (offset < file_size) {
        size_t to_write = (file_size - offset) < chunk_size ? (file_size - offset) : chunk_size;
        written = esp_http_client_write(client, (char*)(file_data + offset), to_write);
        if (written < 0) {
            LOG_E("Failed to write file data at offset %u", offset);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            heap_caps_free(file_data);
            return false;
        }
        offset += written;
    }

    // Write model part
    written = esp_http_client_write(client, model_part.c_str(), model_part.length());
    if (written < 0) {
        LOG_E("Failed to write model part");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        heap_caps_free(file_data);
        return false;
    }

    // Write footer part
    written = esp_http_client_write(client, footer_part.c_str(), footer_part.length());
    if (written < 0) {
        LOG_E("Failed to write footer part");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        heap_caps_free(file_data);
        return false;
    }

    // Free file data buffer (no longer needed)
    heap_caps_free(file_data);

    // Fetch response
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    LOG_I("HTTP Status: %d, Content-Length: %d", status_code, content_length);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status_code != 200) {
        LOG_E("Whisper API returned error status: %d", status_code);
        LOG_E("Response: %s", response_buffer.c_str());
        return false;
    }

    // Parse JSON response: {"text": "transcribed text"}
    cJSON* root = cJSON_Parse(response_buffer.c_str());
    if (!root) {
        LOG_E("Failed to parse JSON response");
        return false;
    }

    cJSON* text = cJSON_GetObjectItem(root, "text");
    if (!text || !cJSON_IsString(text)) {
        LOG_E("Invalid JSON response format (missing 'text' field)");
        cJSON_Delete(root);
        return false;
    }

    transcription = text->valuestring;
    cJSON_Delete(root);

    LOG_I("Transcription: %s", transcription.c_str());
    return true;
}

bool VoiceAssistant::makeGPTRequest(const std::string& prompt, std::string& response) {
    LOG_I("Making Ollama/GPT request");

    // Check if we have WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        LOG_E("WiFi not connected");
        return false;
    }

    // Get endpoint from settings - support both local and cloud modes
    std::string gpt_url;
    const SettingsSnapshot& settings = SettingsManager::getInstance().getSnapshot();

    if (settings.localApiMode) {
        // Local Docker mode: http://<docker_ip>:11434/v1/chat/completions (Ollama)
        gpt_url = "http://" + settings.dockerHostIp + ":11434/v1/chat/completions";
        LOG_I("Using LOCAL Ollama LLM at: %s", gpt_url.c_str());
    } else {
        // Cloud mode: use configured endpoint
        gpt_url = settings.openAiEndpoint + "/chat/completions";
        LOG_I("Using CLOUD GPT API at: %s", gpt_url.c_str());
    }

    // Build JSON request body using cJSON
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        LOG_E("Failed to create JSON object");
        return false;
    }

    // Add model field
    cJSON_AddStringToObject(root, "model", "llama3.2:3b");  // Default model for Ollama

    // Build messages array
    cJSON* messages = cJSON_CreateArray();
    if (!messages) {
        LOG_E("Failed to create messages array");
        cJSON_Delete(root);
        return false;
    }

    // System message: Define the assistant's role
    cJSON* system_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(system_msg, "role", "system");
    cJSON_AddStringToObject(system_msg, "content",
        "You are a voice assistant that converts natural language commands to JSON. "
        "Respond ONLY with valid JSON in this format: {\"command\": \"<command_name>\", \"args\": [\"<arg1>\", \"<arg2>\"]}. "
        "Available commands: volume_up, volume_down, brightness_up, brightness_down, led_brightness, radio_play, wifi_switch. "
        "If the user request doesn't match any command, respond with: {\"command\": \"unknown\", \"args\": []}");
    cJSON_AddItemToArray(messages, system_msg);

    // User message: The transcribed text
    cJSON* user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", prompt.c_str());
    cJSON_AddItemToArray(messages, user_msg);

    cJSON_AddItemToObject(root, "messages", messages);

    // Add optional parameters
    cJSON_AddNumberToObject(root, "temperature", 0.7);
    cJSON_AddBoolToObject(root, "stream", false);

    // Convert JSON to string
    char* json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        LOG_E("Failed to serialize JSON");
        cJSON_Delete(root);
        return false;
    }

    std::string request_body = json_str;
    cJSON_free(json_str);
    cJSON_Delete(root);

    LOG_I("Request body: %s", request_body.c_str());

    // Buffer for response
    std::string response_buffer;
    response_buffer.reserve(2048);

    // HTTP client event handler
    auto http_event_handler = [](esp_http_client_event_t *evt) -> esp_err_t {
        std::string* response = static_cast<std::string*>(evt->user_data);
        switch(evt->event_id) {
            case HTTP_EVENT_ON_DATA:
                if (response && evt->data_len > 0) {
                    response->append((char*)evt->data, evt->data_len);
                }
                break;
            default:
                break;
        }
        return ESP_OK;
    };

    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = gpt_url.c_str();
    config.method = HTTP_METHOD_POST;
    config.event_handler = http_event_handler;
    config.user_data = &response_buffer;
    config.timeout_ms = 15000;  // 15 second timeout for LLM
    config.buffer_size = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        LOG_E("Failed to initialize HTTP client");
        return false;
    }

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Optional: Add API key if using OpenAI cloud (Ollama doesn't need it)
    if (!settings.localApiMode && !settings.openAiApiKey.empty()) {
        std::string auth_header = std::string("Bearer ") + settings.openAiApiKey;
        esp_http_client_set_header(client, "Authorization", auth_header.c_str());
        LOG_I("Using API key for cloud authentication");
    }

    // Set POST data
    esp_http_client_set_post_field(client, request_body.c_str(), request_body.length());

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        LOG_E("HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int status_code = esp_http_client_get_status_code(client);
    LOG_I("HTTP Status: %d", status_code);

    esp_http_client_cleanup(client);

    if (status_code != 200) {
        LOG_E("LLM API returned error status: %d", status_code);
        LOG_E("Response: %s", response_buffer.c_str());
        return false;
    }

    LOG_I("LLM response: %s", response_buffer.c_str());

    // Parse JSON response
    // OpenAI/Ollama format: {"choices": [{"message": {"content": "..."}}]}
    cJSON* resp_root = cJSON_Parse(response_buffer.c_str());
    if (!resp_root) {
        LOG_E("Failed to parse LLM response JSON");
        return false;
    }

    cJSON* choices = cJSON_GetObjectItem(resp_root, "choices");
    if (!choices || !cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        LOG_E("Invalid response format (missing 'choices' array)");
        cJSON_Delete(resp_root);
        return false;
    }

    cJSON* first_choice = cJSON_GetArrayItem(choices, 0);
    if (!first_choice) {
        LOG_E("Invalid response format (empty choices array)");
        cJSON_Delete(resp_root);
        return false;
    }

    cJSON* message = cJSON_GetObjectItem(first_choice, "message");
    if (!message) {
        LOG_E("Invalid response format (missing 'message')");
        cJSON_Delete(resp_root);
        return false;
    }

    cJSON* content = cJSON_GetObjectItem(message, "content");
    if (!content || !cJSON_IsString(content)) {
        LOG_E("Invalid response format (missing 'content')");
        cJSON_Delete(resp_root);
        return false;
    }

    response = content->valuestring;
    cJSON_Delete(resp_root);

    LOG_I("Extracted command JSON: %s", response.c_str());
    return true;
}

bool VoiceAssistant::parseGPTCommand(const std::string& response, VoiceCommand& cmd) {
    LOG_I("Parsing command from LLM response");

    // Parse JSON: {"command": "volume_up", "args": ["arg1", "arg2"]}
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) {
        LOG_E("Failed to parse command JSON");
        return false;
    }

    // Extract command field
    cJSON* command = cJSON_GetObjectItem(root, "command");
    if (!command || !cJSON_IsString(command)) {
        LOG_E("Invalid command format (missing 'command' field)");
        cJSON_Delete(root);
        return false;
    }

    cmd.command = command->valuestring;

    // Extract args field (optional)
    cJSON* args = cJSON_GetObjectItem(root, "args");
    cmd.args.clear();

    if (args && cJSON_IsArray(args)) {
        int args_count = cJSON_GetArraySize(args);
        for (int i = 0; i < args_count; i++) {
            cJSON* arg = cJSON_GetArrayItem(args, i);
            if (arg && cJSON_IsString(arg)) {
                cmd.args.push_back(arg->valuestring);
            }
        }
    }

    cJSON_Delete(root);

    LOG_I("Parsed command: %s (args count: %d)", cmd.command.c_str(), cmd.args.size());
    return true;
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

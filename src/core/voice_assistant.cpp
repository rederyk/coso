#include "core/voice_assistant.h"
#include "core/settings_manager.h"
#include "core/audio_manager.h"
#include "core/microphone_manager.h"
#include "core/command_center.h"
#include "core/conversation_buffer.h"
#include "core/ble_hid_manager.h"
#include "core/web_data_manager.h"
#include "core/memory_manager.h"
#include "peripheral/gpio_manager.h"
#include "utils/logger.h"
#include <esp_http_client.h>
#include <cstring>
#include <sstream>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <cJSON.h>
#include <mutex>

namespace {
thread_local VoiceAssistant::LuaSandbox* s_active_lua_sandbox = nullptr;
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

std::string joinArgs(const std::vector<std::string>& args) {
    std::string joined;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            joined += ", ";
        }
        joined += args[i];
    }
    return joined;
}

std::string formatConversationEntry(const ConversationEntry& entry) {
    std::string content;
    if (!entry.text.empty()) {
        content = entry.text;
    } else if (!entry.transcription.empty()) {
        content = entry.transcription;
    }

    const bool is_assistant = entry.role == "assistant";
    const bool is_user = entry.role == "user";

    if (is_assistant && !entry.command.empty()) {
        if (!content.empty()) {
            content += "\n";
        }
        content += "Command: " + entry.command;
        if (!entry.args.empty()) {
            content += " (";
            content += joinArgs(entry.args);
            content += ")";
        }
    } else if (is_user && !entry.transcription.empty() && entry.transcription != entry.text) {
        if (!content.empty()) {
            content += "\n";
        }
        content += "Transcript: " + entry.transcription;
    }

    if (content.empty() && !entry.command.empty()) {
        content = "Command: " + entry.command;
        if (!entry.args.empty()) {
            content += " (";
            content += joinArgs(entry.args);
            content += ")";
        }
    }

    if (content.empty()) {
        content = entry.transcription;
    }

    return content;
}

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

    // Set initialized_ before creating worker tasks so they don't exit immediately
    initialized_ = true;

    // Create tasks
    // Note: Recording task is created on-demand when startRecording() is called
    BaseType_t stt_result = xTaskCreatePinnedToCore(
        speechToTextTask, "speech_to_text", STT_TASK_STACK,
        this, STT_TASK_PRIORITY, &sttTask_, tskNO_AFFINITY);

    BaseType_t ai_result = xTaskCreatePinnedToCore(
        aiProcessingTask, "ai_processing", AI_TASK_STACK,
        this, AI_TASK_PRIORITY, &aiTask_, tskNO_AFFINITY);

    if (stt_result != pdPASS || ai_result != pdPASS) {
        LOG_E("Failed to create assistant tasks (stt=%d, ai=%d)", stt_result, ai_result);
        end();
        return false;
    }

    LOG_I("Voice assistant initialized successfully (using MicrophoneManager)");
    return true;
}

void VoiceAssistant::end() {
    initialized_ = false;
    if (sttTask_) {
        xTaskNotifyGive(sttTask_);
    }

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

bool VoiceAssistant::isInitialized() const {
    return initialized_;
}

CommandResult VoiceAssistant::executeLuaScript(const std::string& script) {
    std::lock_guard<std::mutex> lock(lua_mutex_);
    return lua_sandbox_.execute(script);
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
    {
        std::lock_guard<std::mutex> lock(last_recorded_mutex_);
        last_recorded_file_.clear();
    }

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

        {
            std::lock_guard<std::mutex> lock(va->last_recorded_mutex_);
            va->last_recorded_file_ = result.file_path;
        }

        {
            std::lock_guard<std::mutex> queue_lock(va->pending_recordings_mutex_);
            va->pending_recordings_.push(result.file_path);
        }

        LOG_I("Audio file ready for STT processing: %s", result.file_path.c_str());

        if (va->sttTask_) {
            xTaskNotifyGive(va->sttTask_);
        }

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
        // Wait until a new recording is queued or we need to shut down
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000)) == 0) {
            continue;
        }

        if (!va->initialized_) {
            break;
        }

        std::string queued_file;
        {
            std::lock_guard<std::mutex> lock(va->pending_recordings_mutex_);
            if (!va->pending_recordings_.empty()) {
                queued_file = va->pending_recordings_.front();
                va->pending_recordings_.pop();
            }
        }

        if (queued_file.empty()) {
            continue;
        }

        LOG_I("Processing audio file for STT: %s", queued_file.c_str());

        std::string transcription;
        bool success = va->makeWhisperRequest(queued_file, transcription);

        if (success && !transcription.empty()) {
            LOG_I("STT successful: %s", transcription.c_str());

            ConversationBuffer::getInstance().addUserMessage(transcription, transcription);

            // Send transcription to AI processing task
            std::string* text_copy = new std::string(transcription);
            if (xQueueSend(va->transcriptionQueue_, &text_copy, pdMS_TO_TICKS(1000)) != pdPASS) {
                LOG_W("Transcription queue full, discarding text");
                delete text_copy;
            }
        } else {
            LOG_E("STT failed or empty transcription");
        }
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
                    cmd.transcription = *transcription;
                    if (va->parseGPTCommand(llm_response, cmd)) {
                        LOG_I("Command parsed successfully: %s (text: %s)", cmd.command.c_str(), cmd.text.c_str());

                        // Check if this is a Lua script command
                        bool is_script_command = (cmd.command == "lua_script" ||
                                                 cmd.command.find("script") != std::string::npos ||
                                                 (!cmd.args.empty() && cmd.args[0].find("function") != std::string::npos));

                        if (is_script_command) {
                            // Execute as Lua script
                            std::string script_content;
                            if (cmd.command == "lua_script" && !cmd.args.empty()) {
                                // Script content is in args
                                script_content = cmd.args[0];
                            } else if (!cmd.args.empty()) {
                                // Try to extract script from args
                                script_content = cmd.args[0];
                            } else {
                                // Use the text field as script
                                script_content = cmd.text;
                            }

                            LOG_I("Executing Lua script: %s", script_content.c_str());
                            CommandResult script_result = va->executeLuaScript(script_content);

                            // Capture script output
                            cmd.output = script_result.message;
                            if (!cmd.output.empty()) {
                                LOG_I("Lua command output: %s", cmd.output.c_str());
                            }

                            if (script_result.success) {
                                LOG_I("Lua script executed successfully: %s", script_result.message.c_str());
                                cmd.text = "Script eseguito con successo. Output: " + script_result.message;
                            } else {
                                LOG_E("Lua script execution failed: %s", script_result.message.c_str());
                                cmd.text = "Errore nell'esecuzione dello script: " + script_result.message;
                            }

                            // Phase 1: Output Refinement - Check if Lua output needs refinement
                            if (script_result.success && !cmd.output.empty()) {
                                cmd.needs_refinement = va->shouldRefineOutput(cmd);

                                if (cmd.needs_refinement) {
                                    LOG_I("Lua output needs refinement, processing...");
                                    bool refined = va->refineCommandOutput(cmd);

                                    if (refined && !cmd.refined_output.empty()) {
                                        // Replace cmd.text with refined output for user display
                                        cmd.text = cmd.refined_output;
                                        LOG_I("Using refined output: %s", cmd.refined_output.c_str());
                                    } else {
                                        LOG_W("Refinement failed, using original output");
                                    }
                                }
                            }
                        } else {
                            // Execute command via CommandCenter only if command is not "none"
                            if (cmd.command != "none" && cmd.command != "unknown" && !cmd.command.empty()) {
                                CommandResult result = CommandCenter::getInstance().executeCommand(cmd.command, cmd.args);

                                // Capture command output
                                cmd.output = result.message;
                                if (!cmd.output.empty()) {
                                    LOG_I("Command output: %s", cmd.output.c_str());
                                }

                                if (result.success) {
                                    LOG_I("Command executed successfully: %s", result.message.c_str());

                                    // Phase 1: Output Refinement - Check if command output needs refinement
                                    if (!cmd.output.empty()) {
                                        cmd.needs_refinement = va->shouldRefineOutput(cmd);

                                        if (cmd.needs_refinement) {
                                            LOG_I("Command output needs refinement, processing...");
                                            bool refined = va->refineCommandOutput(cmd);

                                            if (refined && !cmd.refined_output.empty()) {
                                                // Replace cmd.text with refined output for user display
                                                cmd.text = cmd.refined_output;
                                                LOG_I("Using refined output: %s", cmd.refined_output.c_str());
                                            } else {
                                                LOG_W("Refinement failed, using original output");
                                            }
                                        }
                                    }
                                } else {
                                    LOG_E("Command execution failed: %s", result.message.c_str());
                                }
                            } else {
                                LOG_I("No command to execute (conversational response only)");
                            }
                        }

                        const std::string response_text = cmd.text.empty() ? std::string("Comando elaborato") : cmd.text;
                        ConversationBuffer::getInstance().addAssistantMessage(response_text,
                                                                             cmd.command,
                                                                             cmd.args,
                                                                             cmd.transcription,
                                                                             cmd.output,
                                                                             cmd.refined_output);

                        // Send result to voice command queue for external consumption (screen/web)
                        VoiceCommand* cmd_copy = new VoiceCommand(cmd);
                        if (xQueueSend(va->voiceCommandQueue_, &cmd_copy, pdMS_TO_TICKS(100)) != pdPASS) {
                            LOG_W("Voice command queue full");
                            delete cmd_copy;
                        }
                    } else {
                        LOG_E("Failed to parse command from LLM response");
                        // Fallback: use raw LLM response as text if JSON parsing fails
                        cmd.command = "none";
                        cmd.text = llm_response;
                        cmd.args.clear();

                        LOG_I("Using raw LLM response as fallback text");
                        ConversationBuffer::getInstance().addAssistantMessage(llm_response,
                                                                             "none",
                                                                             std::vector<std::string>(),
                                                                             cmd.transcription,
                                                                             cmd.output);

                        // Send fallback response to queue
                        VoiceCommand* cmd_copy = new VoiceCommand(cmd);
                        if (xQueueSend(va->voiceCommandQueue_, &cmd_copy, pdMS_TO_TICKS(100)) != pdPASS) {
                            LOG_W("Voice command queue full");
                            delete cmd_copy;
                        }
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
bool VoiceAssistant::makeWhisperRequest(const std::string& file_path, std::string& transcription) {
    LOG_I("Making Whisper STT request (file-based implementation)");

    // Check if we have WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        LOG_E("WiFi not connected");
        return false;
    }

    if (file_path.empty()) {
        LOG_E("No recorded file available");
        return false;
    }

    LOG_I("Attempting to open file: '%s'", file_path.c_str());
    LOG_I("File path length: %zu bytes", file_path.length());

    // MicrophoneManager returns paths with "/sd" prefix for SD card.
    // We need to use SD_MMC.open() with the path WITHOUT the "/sd" prefix.
    std::string normalized_path = file_path;
    if (file_path.find("/sd/") == 0) {
        normalized_path = file_path.substr(3);  // Remove "/sd" prefix
        LOG_I("Removed /sd prefix, using path: '%s'", normalized_path.c_str());
    }

    // Open the WAV file using SD_MMC (Arduino File API)
    File file = SD_MMC.open(normalized_path.c_str(), FILE_READ);
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
        // Local mode: use configured local Whisper endpoint
        whisper_url = settings.whisperLocalEndpoint;
        LOG_I("Using LOCAL Whisper API at: %s", whisper_url.c_str());
    } else {
        // Cloud mode: use configured cloud Whisper endpoint
        whisper_url = settings.whisperCloudEndpoint;
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

    // Configure HTTP client
    LOG_I("Configuring HTTP client for URL: %s", whisper_url.c_str());
    LOG_I("Total content length: %u bytes", total_length);

    esp_http_client_config_t config = {};
    config.url = whisper_url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 30000;  // 30 second timeout
    config.buffer_size = 4096;
    config.buffer_size_tx = 4096;

    LOG_I("Initializing HTTP client...");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        LOG_E("Failed to initialize HTTP client");
        heap_caps_free(file_data);
        return false;
    }
    LOG_I("HTTP client initialized successfully");

    // Set headers
    std::string content_type = std::string("multipart/form-data; boundary=") + boundary;
    esp_http_client_set_header(client, "Content-Type", content_type.c_str());
    esp_http_client_set_header(client, "User-Agent", "ESP32-VoiceAssistant/1.0");
    LOG_I("HTTP headers set: Content-Type=%s", content_type.c_str());

    // Optional: Add API key if using OpenAI cloud (local Whisper doesn't need it)
    if (!settings.localApiMode && !settings.openAiApiKey.empty()) {
        std::string auth_header = std::string("Bearer ") + settings.openAiApiKey;
        esp_http_client_set_header(client, "Authorization", auth_header.c_str());
        LOG_I("Using API key for cloud authentication");
    }

    // Open connection
    LOG_I("Opening HTTP connection to %s...", whisper_url.c_str());
    esp_err_t err = esp_http_client_open(client, total_length);
    if (err != ESP_OK) {
        LOG_E("Failed to open HTTP connection: %s (0x%x)", esp_err_to_name(err), err);
        LOG_E("WiFi status: %d", WiFi.status());
        LOG_E("Check if server is reachable and port is correct");
        esp_http_client_cleanup(client);
        heap_caps_free(file_data);
        return false;
    }
    LOG_I("HTTP connection opened successfully");

    // Write multipart body in chunks
    int written = 0;

    // Write header part
    LOG_I("Writing multipart header (%u bytes)...", header_part.length());
    written = esp_http_client_write(client, header_part.c_str(), header_part.length());
    if (written < 0) {
        LOG_E("Failed to write header part");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        heap_caps_free(file_data);
        return false;
    }
    LOG_I("Header written successfully");

    // Write file data in chunks
    LOG_I("Writing audio file data (%u bytes)...", file_size);
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
    LOG_I("Audio file data written successfully (%u bytes)", offset);

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
    LOG_I("Fetching HTTP response headers...");
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    // Read response body manually
    std::string response_buffer;
    response_buffer.reserve(1024);
    char read_buf[512];
    int read_len = 0;
    while ((read_len = esp_http_client_read(client, read_buf, sizeof(read_buf))) > 0) {
        response_buffer.append(read_buf, read_len);
    }

    LOG_I("HTTP Status: %d, Content-Length: %d", status_code, content_length);
    LOG_I("Response buffer size: %u bytes", response_buffer.length());

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
    SettingsManager& settings_manager = SettingsManager::getInstance();
    SettingsSnapshot settings = settings_manager.getSnapshot();
    std::string gpt_url;

    if (settings.localApiMode) {
        // Local mode: use configured local LLM endpoint
        gpt_url = settings.llmLocalEndpoint;
        LOG_I("Using LOCAL LLM at: %s", gpt_url.c_str());
    } else {
        // Cloud mode: use configured cloud LLM endpoint
        gpt_url = settings.llmCloudEndpoint;
        LOG_I("Using CLOUD LLM API at: %s", gpt_url.c_str());
    }

    std::string selected_model = settings.llmModel;
    if (selected_model.empty() && settings.localApiMode) {
        std::vector<std::string> available;
        if (fetchOllamaModels(settings.llmLocalEndpoint, available) && !available.empty()) {
            selected_model = available.front();
            settings_manager.setLlmModel(selected_model);
            settings.llmModel = selected_model;
            LOG_I("Selected default local model: %s", selected_model.c_str());
        }
    }

    if (selected_model.empty()) {
        LOG_E("No LLM model configured");
        return false;
    }

    const std::string system_prompt = getSystemPrompt();
    const std::vector<ConversationEntry> conversation_history = ConversationBuffer::getInstance().getEntries();
    const bool prompt_recorded = [&]() {
        if (conversation_history.empty()) {
            return false;
        }
        const ConversationEntry& last = conversation_history.back();
        if (last.role != "user") {
            return false;
        }
        if (!last.text.empty()) {
            return last.text == prompt;
        }
        return last.transcription == prompt;
    }();

    auto buildRequestBody = [&](const std::string& model, std::string& out) -> bool {
        // Build JSON request body using cJSON
        cJSON* root = cJSON_CreateObject();
        if (!root) {
            LOG_E("Failed to create JSON object");
            return false;
        }

        cJSON_AddStringToObject(root, "model", model.c_str());

        cJSON* messages = cJSON_CreateArray();
        if (!messages) {
            LOG_E("Failed to create messages array");
            cJSON_Delete(root);
            return false;
        }

        cJSON* system_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(system_msg, "role", "system");
        cJSON_AddStringToObject(system_msg, "content", system_prompt.c_str());
        cJSON_AddItemToArray(messages, system_msg);

        auto append_message = [&](const std::string& role, const std::string& content) -> bool {
            if (content.empty()) {
                return true;
            }
            cJSON* msg = cJSON_CreateObject();
            if (!msg) {
                return false;
            }
            cJSON_AddStringToObject(msg, "role", role.c_str());
            cJSON_AddStringToObject(msg, "content", content.c_str());
            cJSON_AddItemToArray(messages, msg);
            return true;
        };

        for (const auto& entry : conversation_history) {
            const std::string role = entry.role == "assistant" ? "assistant" : "user";
            const std::string content = formatConversationEntry(entry);
            if (!append_message(role, content)) {
                LOG_E("Failed to append conversation entry to request");
                cJSON_Delete(root);
                return false;
            }
        }

        if (!prompt_recorded) {
            if (!append_message("user", prompt)) {
                LOG_E("Failed to append fallback user prompt");
                cJSON_Delete(root);
                return false;
            }
        }

        cJSON_AddItemToObject(root, "messages", messages);
        cJSON_AddNumberToObject(root, "temperature", 0.7);
        cJSON_AddBoolToObject(root, "stream", false);

        char* json_str = cJSON_PrintUnformatted(root);
        if (!json_str) {
            LOG_E("Failed to serialize JSON");
            cJSON_Delete(root);
            return false;
        }

        out.assign(json_str);
        cJSON_free(json_str);
        cJSON_Delete(root);
        return true;
    };

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

    bool fallback_attempted = false;
    std::string request_body;

    while (true) {
        if (!buildRequestBody(selected_model, request_body)) {
            return false;
        }

        LOG_I("Request body: %s", request_body.c_str());
        response_buffer.clear();

        // Configure HTTP client
        esp_http_client_config_t config = {};
        config.url = gpt_url.c_str();
        config.method = HTTP_METHOD_POST;
        config.event_handler = http_event_handler;
        config.user_data = &response_buffer;
        config.timeout_ms = 45000;  // 45 second timeout for LLM (first model load can take 30-40s)
        config.buffer_size = 8192;  // Increased buffer for large responses
        config.buffer_size_tx = 8192;

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

        if (status_code == 404 && settings.localApiMode && !fallback_attempted) {
            LOG_W("Model '%s' not available on local endpoint, attempting fallback", selected_model.c_str());
            std::vector<std::string> available;
            if (fetchOllamaModels(settings.llmLocalEndpoint, available) && !available.empty()) {
                const std::string previous_model = selected_model;
                selected_model = available.front();
                settings_manager.setLlmModel(selected_model);
                settings.llmModel = selected_model;
                fallback_attempted = true;
                LOG_I("Falling back from %s to %s and retrying request",
                      previous_model.c_str(), selected_model.c_str());
                continue;
            } else {
                LOG_E("Failed to retrieve fallback models from Ollama");
            }
        }

        if (status_code != 200) {
            LOG_E("LLM API returned error status: %d", status_code);
            LOG_E("Response: %s", response_buffer.c_str());
            return false;
        }

        LOG_I("LLM response: %s", response_buffer.c_str());
        break;
    }

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

    // Parse JSON: {"command": "volume_up", "args": ["arg1", "arg2"], "text": "Ho aumentato il volume"}
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

    // Extract text field (conversational response)
    cJSON* text = cJSON_GetObjectItem(root, "text");
    if (text && cJSON_IsString(text)) {
        cmd.text = text->valuestring;
    } else {
        cmd.text = "";  // Empty if not provided
    }

    cJSON_Delete(root);

    LOG_I("Parsed command: %s (args count: %d, text: %s)",
          cmd.command.c_str(), cmd.args.size(), cmd.text.c_str());
    return true;
}

// Ollama API helpers
bool VoiceAssistant::fetchOllamaModels(const std::string& base_url, std::vector<std::string>& models) {
    LOG_I("Fetching available models from Ollama API");

    models.clear();

    // Check if we have WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        LOG_E("WiFi not connected");
        return false;
    }

    // Parse base URL to extract host and build /api/tags endpoint
    // Expected format: http://192.168.1.51:11434/v1/chat/completions
    // We need: http://192.168.1.51:11434/api/tags
    std::string ollama_tags_url = base_url;

    // Remove trailing /v1/chat/completions or similar paths
    size_t v1_pos = ollama_tags_url.find("/v1/");
    if (v1_pos != std::string::npos) {
        ollama_tags_url = ollama_tags_url.substr(0, v1_pos);
    }

    // Ensure no trailing slash
    if (!ollama_tags_url.empty() && ollama_tags_url.back() == '/') {
        ollama_tags_url.pop_back();
    }

    // Append /api/tags
    ollama_tags_url += "/api/tags";

    LOG_I("Ollama tags endpoint: %s", ollama_tags_url.c_str());

    // Buffer for response
    std::string response_buffer;
    response_buffer.reserve(4096);

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
    config.url = ollama_tags_url.c_str();
    config.method = HTTP_METHOD_GET;
    config.event_handler = http_event_handler;
    config.user_data = &response_buffer;
    config.timeout_ms = 10000;  // 10 second timeout
    config.buffer_size = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        LOG_E("Failed to initialize HTTP client");
        return false;
    }

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
        LOG_E("Ollama API returned error status: %d", status_code);
        LOG_E("Response: %s", response_buffer.c_str());
        return false;
    }

    LOG_I("Ollama response: %s", response_buffer.c_str());

    // Parse JSON response
    // Ollama format: {"models": [{"name": "llama3.2:3b", ...}, {"name": "mistral:7b", ...}]}
    cJSON* root = cJSON_Parse(response_buffer.c_str());
    if (!root) {
        LOG_E("Failed to parse Ollama response JSON");
        return false;
    }

    cJSON* models_array = cJSON_GetObjectItem(root, "models");
    if (!models_array || !cJSON_IsArray(models_array)) {
        LOG_E("Invalid response format (missing 'models' array)");
        cJSON_Delete(root);
        return false;
    }

    int model_count = cJSON_GetArraySize(models_array);
    LOG_I("Found %d models", model_count);

    for (int i = 0; i < model_count; i++) {
        cJSON* model_item = cJSON_GetArrayItem(models_array, i);
        if (!model_item) continue;

        cJSON* name = cJSON_GetObjectItem(model_item, "name");
        if (name && cJSON_IsString(name)) {
            models.push_back(name->valuestring);
            LOG_I("  - %s", name->valuestring);
        }
    }

    cJSON_Delete(root);

    if (models.empty()) {
        LOG_W("No models found in Ollama API response");
        return false;
    }

    LOG_I("Successfully fetched %d models from Ollama", models.size());
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

// Output refinement helpers (Phase 1: Output Refinement System)
bool VoiceAssistant::shouldRefineOutput(const VoiceCommand& cmd) {
    // Criteria to determine if output needs refinement:

    // 1. Empty output - no refinement needed
    if (cmd.output.empty()) {
        return false;
    }

    // 2. Output is very long (>200 characters) - likely technical data
    if (cmd.output.size() > 200) {
        LOG_I("Output exceeds 200 chars (%zu), needs refinement", cmd.output.size());
        return true;
    }

    // 3. Output contains JSON structure
    if (cmd.output.find('{') != std::string::npos ||
        cmd.output.find('[') != std::string::npos) {
        LOG_I("Output contains JSON, needs refinement");
        return true;
    }

    // 4. Lua script commands that use webData (likely produce raw data)
    if (cmd.command == "lua_script" &&
        !cmd.args.empty() &&
        (cmd.args[0].find("webData") != std::string::npos ||
         cmd.args[0].find("memory.read") != std::string::npos)) {
        LOG_I("Lua webData/memory command, needs refinement");
        return true;
    }

    // 5. Commands that typically produce technical output
    if (cmd.command == "heap" ||
        cmd.command == "system_status" ||
        cmd.command == "log_tail") {
        LOG_I("Technical command '%s', needs refinement", cmd.command.c_str());
        return true;
    }

    return false;
}

std::string VoiceAssistant::buildRefinementPrompt(const VoiceCommand& cmd) {
    std::string prompt;

    // Context: what did the user ask?
    if (!cmd.transcription.empty()) {
        prompt += "L'utente ha chiesto: \"" + cmd.transcription + "\"\n\n";
    }

    // What command was executed?
    if (!cmd.command.empty()) {
        prompt += "Il sistema ha eseguito il comando: " + cmd.command;
        if (!cmd.args.empty() && !cmd.args[0].empty()) {
            // Show only first 100 chars of args to avoid huge prompts
            std::string args_preview = cmd.args[0];
            if (args_preview.size() > 100) {
                args_preview = args_preview.substr(0, 100) + "...";
            }
            prompt += " (argomento: " + args_preview + ")";
        }
        prompt += "\n\n";
    }

    // The raw output that needs refinement
    prompt += "Il comando ha prodotto questo output tecnico:\n";
    prompt += "```\n" + cmd.output + "\n```\n\n";

    // Instructions for refinement
    prompt += "Il tuo compito è riassumere questo output in modo comprensibile e user-friendly.\n";
    prompt += "Linee guida:\n";
    prompt += "- Se sono dati meteo (temperatura, vento, ecc.), formattali in modo naturale\n";
    prompt += "- Se è JSON, estrai solo le informazioni rilevanti per l'utente\n";
    prompt += "- Se sono log o dati tecnici, riassumili in 1-2 frasi chiare\n";
    prompt += "- Usa un tono conversazionale e amichevole\n";
    prompt += "- NON includere dettagli tecnici inutili (chiavi JSON, coordinate esatte, ecc.)\n";
    prompt += "- Rispondi SOLO con il testo formattato, niente altro\n";

    return prompt;
}

bool VoiceAssistant::refineCommandOutput(VoiceCommand& cmd) {
    if (cmd.output.empty() || !cmd.needs_refinement) {
        LOG_W("refineCommandOutput called but output empty or doesn't need refinement");
        return false;
    }

    LOG_I("Refining command output (size: %zu bytes)", cmd.output.size());

    // Build refinement prompt
    std::string refinement_prompt = buildRefinementPrompt(cmd);

    // Call GPT to refine the output
    std::string refined;
    bool success = makeGPTRequest(refinement_prompt, refined);

    if (success && !refined.empty()) {
        cmd.refined_output = refined;
        LOG_I("Output refined successfully: %s", refined.c_str());
        return true;
    } else {
        LOG_E("Failed to refine output (GPT request failed or empty response)");
        return false;
    }
}

// Public API for chat interface
bool VoiceAssistant::sendTextMessage(const std::string& text) {
    if (!initialized_) {
        LOG_E("VoiceAssistant not initialized");
        return false;
    }

    if (text.empty()) {
        LOG_E("Empty text message");
        return false;
    }

    LOG_I("Sending text message to LLM: %s", text.c_str());

    ConversationBuffer::getInstance().addUserMessage(text);

    // Send text directly to transcription queue (bypass STT)
    std::string* text_copy = new std::string(text);
    if (xQueueSend(transcriptionQueue_, &text_copy, pdMS_TO_TICKS(1000)) != pdPASS) {
        LOG_W("Transcription queue full, discarding text");
        delete text_copy;
        return false;
    }

    return true;
}

bool VoiceAssistant::getLastResponse(VoiceCommand& response, uint32_t timeout_ms) {
    if (!initialized_) {
        LOG_E("VoiceAssistant not initialized");
        return false;
    }

    VoiceCommand* cmd = nullptr;
    if (xQueueReceive(voiceCommandQueue_, &cmd, pdMS_TO_TICKS(timeout_ms)) == pdPASS) {
        if (cmd) {
            response = *cmd;
            delete cmd;
            return true;
        }
    }

    return false;
}

std::string VoiceAssistant::getLastRecordedFile() const {
    std::lock_guard<std::mutex> lock(last_recorded_mutex_);
    return last_recorded_file_;
}

std::string VoiceAssistant::getSystemPrompt() const {
    const SettingsSnapshot& settings = SettingsManager::getInstance().getSnapshot();
    std::string prompt_template = settings.voiceAssistantSystemPromptTemplate.empty()
        ? VOICE_ASSISTANT_PROMPT_TEMPLATE
        : settings.voiceAssistantSystemPromptTemplate;

    auto commands = CommandCenter::getInstance().listCommands();
    std::string command_list;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0) {
            command_list += "; ";
        }
        command_list += commands[i].name;
        if (!commands[i].description.empty()) {
            command_list += " (" + commands[i].description + ")";
        }
    }

    if (command_list.empty()) {
        command_list = "none";
    }

    std::string host_list;
    BleHidManager& ble = BleHidManager::getInstance();
    if (!ble.isInitialized()) {
        host_list = "unavailable (BLE not initialized)";
    } else {
        auto bonded_peers = ble.getBondedPeers();
        if (bonded_peers.empty()) {
            host_list = "none";
        } else {
            for (size_t i = 0; i < bonded_peers.size(); ++i) {
                if (i > 0) {
                    host_list += ", ";
                }
                host_list += bonded_peers[i].address.toString();
                host_list += bonded_peers[i].isConnected ? " (connected)" : " (not connected)";
            }
        }
    }

    std::string prompt = prompt_template;
    const std::string placeholder = VOICE_ASSISTANT_COMMAND_LIST_PLACEHOLDER;
    const size_t pos = prompt.find(placeholder);
    if (pos != std::string::npos) {
        prompt.replace(pos, placeholder.length(), command_list);
    } else {
        prompt += " Available commands: " + command_list;
    }

    const std::string hosts_placeholder = VOICE_ASSISTANT_BLE_HOSTS_PLACEHOLDER;
    const size_t hosts_pos = prompt.find(hosts_placeholder);
    if (hosts_pos != std::string::npos) {
        prompt.replace(hosts_pos, hosts_placeholder.length(), host_list);
    } else {
        prompt += " Bonded BLE hosts: " + host_list + ".";
    }

    // Add Lua scripting capabilities
    prompt += "\n\nInoltre puoi utilizzare script Lua per operazioni complesse combinando GPIO, BLE, audio, web data e altre funzioni. ";
    prompt += "Per eseguire uno script Lua, usa il comando 'lua_script' con lo script come argomento. ";
    prompt += "API Lua disponibili:\n";
    prompt += "- GPIO: gpio.write(pin, value), gpio.read(pin), gpio.toggle(pin)\n";
    prompt += "- BLE: ble.type(host_mac, text), ble.send_key(host_mac, keycode, modifier), ble.mouse_move(host_mac, dx, dy), ble.click(host_mac, buttons)\n";
    prompt += "- Audio: audio.volume_up(), audio.volume_down()\n";
    prompt += "- Display: display.brightness_up(), display.brightness_down()\n";
    prompt += "- LED: led.set_brightness(percentage)\n";
    prompt += "- WebData: webData.fetch_once(url, filename), webData.fetch_scheduled(url, filename, minutes), webData.read_data(filename), webData.list_files()\n";
    prompt += "- Memory: memory.read_file(filename), memory.write_file(filename, data), memory.list_files(), memory.delete_file(filename)\n";
    prompt += "  (l'accesso ai file è limitato alle directory whitelist configurate; usa preferibilmente `/webdata` o `/memory` e lascia che venga eseguito `StorageAccessManager`)\n";
    prompt += "- Sistema: system.ping(), system.uptime(), system.heap(), system.sd_status(), system.status()\n";
    prompt += "- Timing: delay(ms)\n";
    prompt += "Esempi:\n";
    prompt += "- Scarica dati meteo: {\"command\": \"lua_script\", \"args\": [\"webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current_weather=true', 'weather.json')\"], \"text\": \"Dati meteo scaricati\"}\n";
    prompt += "- Leggi dati salvati: {\"command\": \"lua_script\", \"args\": [\"local data = webData.read_data('weather.json'); println('Dati:', data)\"], \"text\": \"Dati letti\"}\n";
    prompt += "- LED lampeggiante: {\"command\": \"lua_script\", \"args\": [\"gpio.write(23, true); delay(1000); gpio.write(23, false)\"], \"text\": \"LED lampeggiato\"}\n";
    prompt += "- Scrivi testo BLE: {\"command\": \"lua_script\", \"args\": [\"ble.type('AA:BB:CC:DD:EE:FF', 'Ciao mondo!')\"], \"text\": \"Testo inviato via BLE\"}\n";
    prompt += "- Sequenza complessa: {\"command\": \"lua_script\", \"args\": [\"gpio.write(23, true); ble.type('AA:BB:CC:DD:EE:FF', 'LED acceso'); delay(2000); gpio.write(23, false); audio.volume_up()\"], \"text\": \"Sequenza completata\"}";

    return prompt;
}

// LuaSandbox implementation
VoiceAssistant::LuaSandbox::LuaSandbox() : L(nullptr) {
    L = luaL_newstate();
    if (L) {
        luaL_openlibs(L);
        setupSandbox();
    } else {
        LOG_E("Failed to create Lua state");
    }
}

VoiceAssistant::LuaSandbox::~LuaSandbox() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

void VoiceAssistant::LuaSandbox::setupSandbox() {
    if (!L) return;

    // Remove dangerous functions
    luaL_dostring(L, R"(
        -- Disable dangerous functions
        os.execute = nil
        io.popen = nil
        os.remove = nil
        os.rename = nil
        loadfile = nil
        dofile = nil

        -- Safe GPIO API
        gpio = {
            write = function(pin, value)
                return esp32_gpio_write(pin, value)
            end,
            read = function(pin)
                return esp32_gpio_read(pin)
            end,
            toggle = function(pin)
                return esp32_gpio_toggle(pin)
            end
        }

        -- Safe timing
        delay = function(ms)
            esp32_delay(ms)
        end

        -- BLE API
        ble = {
            type = function(host_mac, text)
                return esp32_ble_type(host_mac, text)
            end,
            send_key = function(host_mac, keycode, modifier)
                return esp32_ble_send_key(host_mac, keycode, modifier or 0)
            end,
            mouse_move = function(host_mac, dx, dy, wheel, buttons)
                return esp32_ble_mouse_move(host_mac, dx or 0, dy or 0, wheel or 0, buttons or 0)
            end,
            click = function(host_mac, buttons)
                return esp32_ble_click(host_mac, buttons)
            end
        }

        -- Audio API
        audio = {
            volume_up = function()
                return esp32_volume_up()
            end,
            volume_down = function()
                return esp32_volume_down()
            end
        }

        -- Display API
        display = {
            brightness_up = function()
                return esp32_brightness_up()
            end,
            brightness_down = function()
                return esp32_brightness_down()
            end
        }

        -- LED API
        led = {
            set_brightness = function(percentage)
                return esp32_led_brightness(percentage)
            end
        }

        -- System API
        system = {
            ping = function()
                return esp32_ping()
            end,
            uptime = function()
                return esp32_uptime()
            end,
            heap = function()
                return esp32_heap()
            end,
            sd_status = function()
                return esp32_sd_status()
            end,
            status = function()
                return esp32_system_status()
            end
        }

        -- WebData API
        webData = {
            fetch_once = function(url, filename)
                return esp32_webdata_fetch_once(url, filename)
            end,
            fetch_scheduled = function(url, filename, minutes)
                return esp32_webdata_fetch_scheduled(url, filename, minutes)
            end,
            read_data = function(filename)
                return esp32_webdata_read_data(filename)
            end,
            list_files = function()
                return esp32_webdata_list_files()
            end
        }

        -- Memory API
        memory = {
            read_file = function(filename)
                return esp32_memory_read_file(filename)
            end,
            write_file = function(filename, data)
                return esp32_memory_write_file(filename, data)
            end,
            list_files = function()
                return esp32_memory_list_files()
            end,
            delete_file = function(filename)
                return esp32_memory_delete_file(filename)
            end
        }
    )");

    // Register C functions
    lua_register(L, "esp32_gpio_write", lua_gpio_write);
    lua_register(L, "esp32_gpio_read", lua_gpio_read);
    lua_register(L, "esp32_gpio_toggle", lua_gpio_toggle);
    lua_register(L, "esp32_delay", lua_delay);

    // BLE functions
    lua_register(L, "esp32_ble_type", lua_ble_type);
    lua_register(L, "esp32_ble_send_key", lua_ble_send_key);
    lua_register(L, "esp32_ble_mouse_move", lua_ble_mouse_move);
    lua_register(L, "esp32_ble_click", lua_ble_click);

    // Audio functions
    lua_register(L, "esp32_volume_up", lua_volume_up);
    lua_register(L, "esp32_volume_down", lua_volume_down);

    // Display functions
    lua_register(L, "esp32_brightness_up", lua_brightness_up);
    lua_register(L, "esp32_brightness_down", lua_brightness_down);

    // LED functions
    lua_register(L, "esp32_led_brightness", lua_led_brightness);

    // System functions
    lua_register(L, "esp32_ping", lua_ping);
    lua_register(L, "esp32_uptime", lua_uptime);
    lua_register(L, "esp32_heap", lua_heap);
    lua_register(L, "esp32_sd_status", lua_sd_status);
    lua_register(L, "esp32_system_status", lua_system_status);

    // WebData functions
    lua_register(L, "esp32_webdata_fetch_once", lua_webdata_fetch_once);
    lua_register(L, "esp32_webdata_fetch_scheduled", lua_webdata_fetch_scheduled);
    lua_register(L, "esp32_webdata_read_data", lua_webdata_read_data);
    lua_register(L, "esp32_webdata_list_files", lua_webdata_list_files);

    // Memory functions
    lua_register(L, "esp32_memory_read_file", lua_memory_read_file);
    lua_register(L, "esp32_memory_write_file", lua_memory_write_file);
    lua_register(L, "esp32_memory_list_files", lua_memory_list_files);
    lua_register(L, "esp32_memory_delete_file", lua_memory_delete_file);

    // Utility functions
    lua_register(L, "println", lua_println);
}

std::string VoiceAssistant::LuaSandbox::preprocessScript(const std::string& script) {
    std::string processed = script;
    std::string output;
    size_t pos = 0;
    int line_num = 1;

    Serial.println("[LUA] Preprocessing script...");

    // Replace null with nil (JavaScript -> Lua)
    size_t null_pos = 0;
    while ((null_pos = processed.find("null", null_pos)) != std::string::npos) {
        processed.replace(null_pos, 4, "nil");
        null_pos += 3;
    }

    // Process line by line to filter invalid calls
    std::istringstream stream(processed);
    std::string line;
    int valid_lines = 0;
    int skipped_lines = 0;

    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");

        if (start == std::string::npos) {
            // Empty line, keep it
            output += "\n";
            line_num++;
            continue;
        }

        std::string trimmed = line.substr(start, end - start + 1);

        // Check if line is a valid API call or control structure
        bool is_valid = false;

        // Valid prefixes (API namespaces)
        const char* valid_prefixes[] = {
            "gpio.", "ble.", "led.", "audio.", "display.", "system.",
            "delay(", "println(",
            "if ", "else", "end", "for ", "while ", "do", "then",
            "local ", "function ", "return",
            "--"  // Comments
        };

        for (const char* prefix : valid_prefixes) {
            if (trimmed.find(prefix) == 0 || trimmed.find(prefix) != std::string::npos) {
                is_valid = true;
                break;
            }
        }

        // Check for assignments (local var = ...)
        if (trimmed.find("=") != std::string::npos && trimmed.find("local") != std::string::npos) {
            is_valid = true;
        }

        // Check for closing braces/parentheses
        if (trimmed == "}" || trimmed == ")" || trimmed == "end") {
            is_valid = true;
        }

        if (is_valid) {
            output += line + "\n";
            valid_lines++;
            Serial.printf("[LUA] Line %d: VALID - %s\n", line_num, trimmed.c_str());
        } else {
            // Try to parse as pcall to catch errors
            output += "pcall(function() " + line + " end)\n";
            skipped_lines++;
            Serial.printf("[LUA] Line %d: WRAPPED - %s\n", line_num, trimmed.c_str());
        }

        line_num++;
    }

    Serial.printf("[LUA] Preprocessing complete: %d valid, %d wrapped\n", valid_lines, skipped_lines);
    Serial.println("[LUA] Processed script:");
    Serial.println(output.c_str());

    return output;
}

CommandResult VoiceAssistant::LuaSandbox::execute(const std::string& script) {
    if (!L) {
        return {false, "Lua state not initialized"};
    }

    // Preprocess script to fix common LLM mistakes
    std::string processed_script = preprocessScript(script);

    output_buffer_.clear();
    struct SandboxActivation {
        LuaSandbox* previous;
        SandboxActivation(LuaSandbox* current) : previous(s_active_lua_sandbox) {
            s_active_lua_sandbox = current;
        }
        ~SandboxActivation() {
            s_active_lua_sandbox = previous;
        }
    } activation(this);

    int result = luaL_dostring(L, processed_script.c_str());

    if (result != LUA_OK) {
        const char* error = lua_tostring(L, -1);
        std::string error_msg = "Lua error: ";
        error_msg += error ? error : "unknown";
        lua_pop(L, 1); // Remove error message from stack

        Serial.printf("[LUA] Execution error: %s\n", error_msg.c_str());
        if (!output_buffer_.empty()) {
            error_msg += "\nOutput:\n";
            error_msg += output_buffer_;
        }
        return {false, error_msg};
    }

    Serial.println("[LUA] Script executed successfully");
    const std::string message = output_buffer_.empty() ? "Lua script executed" : output_buffer_;
    return {true, message};
}

int VoiceAssistant::LuaSandbox::lua_gpio_write(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    bool value = lua_toboolean(L, 2);

    auto* gpio = GPIOManager::getInstance()->requestGPIO(
        pin, PERIPH_GPIO_OUTPUT, "lua"
    );

    if (gpio) {
        gpio->write(value);
        GPIOManager::getInstance()->releaseGPIO(pin, "lua");
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }

    return 1;
}

// Memory functions
int VoiceAssistant::LuaSandbox::lua_memory_read_file(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);

    auto& memory = MemoryManager::getInstance();
    std::string data = memory.readData(filename);

    if (data.empty()) {
        lua_pushnil(L);
        lua_pushstring(L, "File not found or empty");
        return 2;
    }

    lua_pushstring(L, data.c_str());
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_memory_write_file(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    const char* data = luaL_checkstring(L, 2);

    auto& memory = MemoryManager::getInstance();
    bool success = memory.writeData(filename, data);

    lua_pushboolean(L, success);
    if (!success) {
        lua_pushstring(L, "Failed to write file");
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_memory_list_files(lua_State* L) {
    auto& memory = MemoryManager::getInstance();
    std::vector<std::string> files = memory.listFiles();

    std::ostringstream oss;
    if (files.empty()) {
        oss << "Memory directory is empty";
    } else {
        oss << "Memory files:";
        for (const auto& file : files) {
            oss << "\n- " << file;
        }
    }
    const std::string output = oss.str();
    Serial.println(output.c_str());
    if (s_active_lua_sandbox) {
        s_active_lua_sandbox->appendOutput(output);
    }

    lua_newtable(L);
    for (size_t i = 0; i < files.size(); ++i) {
        lua_pushinteger(L, i + 1);  // Lua arrays are 1-indexed
        lua_pushstring(L, files[i].c_str());
        lua_settable(L, -3);
    }

    return 1;
}

int VoiceAssistant::LuaSandbox::lua_memory_delete_file(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);

    auto& memory = MemoryManager::getInstance();
    bool success = memory.deleteData(filename);

    lua_pushboolean(L, success);
    if (!success) {
        lua_pushstring(L, "Failed to delete file");
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_gpio_read(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);

    auto* gpio = GPIOManager::getInstance()->requestGPIO(
        pin, PERIPH_GPIO_INPUT, "lua"
    );

    if (gpio) {
        bool value = gpio->read();
        GPIOManager::getInstance()->releaseGPIO(pin, "lua");
        lua_pushboolean(L, value);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

int VoiceAssistant::LuaSandbox::lua_gpio_toggle(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);

    auto* gpio = GPIOManager::getInstance()->requestGPIO(
        pin, PERIPH_GPIO_OUTPUT, "lua"
    );

    if (gpio) {
        gpio->toggle();
        GPIOManager::getInstance()->releaseGPIO(pin, "lua");
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }

    return 1;
}

int VoiceAssistant::LuaSandbox::lua_delay(lua_State* L) {
    int ms = luaL_checkinteger(L, 1);
    delay(ms);
    return 0;
}

// BLE functions
int VoiceAssistant::LuaSandbox::lua_ble_type(lua_State* L) {
    const char* host_mac = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);

    CommandResult result = CommandCenter::getInstance().executeCommand("bt_type", {host_mac, text});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_ble_send_key(lua_State* L) {
    const char* host_mac = luaL_checkstring(L, 1);
    int keycode = luaL_checkinteger(L, 2);
    int modifier = luaL_optinteger(L, 3, 0);

    CommandResult result = CommandCenter::getInstance().executeCommand("bt_send_key",
        {host_mac, std::to_string(keycode), std::to_string(modifier)});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_ble_mouse_move(lua_State* L) {
    const char* host_mac = luaL_checkstring(L, 1);
    int dx = luaL_optinteger(L, 2, 0);
    int dy = luaL_optinteger(L, 3, 0);
    int wheel = luaL_optinteger(L, 4, 0);
    int buttons = luaL_optinteger(L, 5, 0);

    CommandResult result = CommandCenter::getInstance().executeCommand("bt_mouse_move",
        {host_mac, std::to_string(dx), std::to_string(dy), std::to_string(wheel), std::to_string(buttons)});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_ble_click(lua_State* L) {
    const char* host_mac = luaL_checkstring(L, 1);
    int buttons = luaL_checkinteger(L, 2);

    CommandResult result = CommandCenter::getInstance().executeCommand("bt_click",
        {host_mac, std::to_string(buttons)});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

// Audio functions
int VoiceAssistant::LuaSandbox::lua_volume_up(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("volume_up", {});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_volume_down(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("volume_down", {});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

// Display functions
int VoiceAssistant::LuaSandbox::lua_brightness_up(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("brightness_up", {});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_brightness_down(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("brightness_down", {});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

// LED functions
int VoiceAssistant::LuaSandbox::lua_led_brightness(lua_State* L) {
    int percentage = luaL_checkinteger(L, 1);

    CommandResult result = CommandCenter::getInstance().executeCommand("led_brightness",
        {std::to_string(percentage)});
    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    return 1;
}

// System functions
int VoiceAssistant::LuaSandbox::lua_ping(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("ping", {});
    lua_pushboolean(L, result.success);
    if (result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    lua_pushstring(L, result.message.c_str());
    return 2;
}

int VoiceAssistant::LuaSandbox::lua_uptime(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("uptime", {});
    lua_pushboolean(L, result.success);
    if (result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    lua_pushstring(L, result.message.c_str());
    return 2;
}

int VoiceAssistant::LuaSandbox::lua_heap(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("heap", {});
    lua_pushboolean(L, result.success);
    if (result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    lua_pushstring(L, result.message.c_str());
    return 2;
}

int VoiceAssistant::LuaSandbox::lua_sd_status(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("sd_status", {});
    lua_pushboolean(L, result.success);
    if (result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    lua_pushstring(L, result.message.c_str());
    return 2;
}

int VoiceAssistant::LuaSandbox::lua_system_status(lua_State* L) {
    CommandResult result = CommandCenter::getInstance().executeCommand("system_status", {});
    lua_pushboolean(L, result.success);
    if (result.success) {
        lua_pushstring(L, result.message.c_str());
        return 2;
    }
    lua_pushstring(L, result.message.c_str());
    return 2;
}

int VoiceAssistant::LuaSandbox::lua_println(lua_State* L) {
    // Get the number of arguments
    int n = lua_gettop(L);
    std::string message;

    // Concatenate all arguments
    for (int i = 1; i <= n; i++) {
        if (lua_isstring(L, i)) {
            if (i > 1) message += " ";
            message += lua_tostring(L, i);
        } else if (lua_isnumber(L, i)) {
            if (i > 1) message += " ";
            message += std::to_string(lua_tonumber(L, i));
        } else if (lua_isboolean(L, i)) {
            if (i > 1) message += " ";
            message += lua_toboolean(L, i) ? "true" : "false";
        } else if (lua_isnil(L, i)) {
            if (i > 1) message += " ";
            message += "nil";
        }
    }

    // Log the message
    Serial.println(message.c_str());

    if (!message.empty() && s_active_lua_sandbox) {
        s_active_lua_sandbox->appendOutput(message);
    }

    return 0;  // No return values
}

void VoiceAssistant::LuaSandbox::appendOutput(const std::string& text) {
    if (text.empty()) {
        return;
    }
    if (!output_buffer_.empty()) {
        output_buffer_ += '\n';
    }
    output_buffer_ += text;
}

// WebData functions
int VoiceAssistant::LuaSandbox::lua_webdata_fetch_once(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    const char* filename = luaL_checkstring(L, 2);

    auto& webData = WebDataManager::getInstance();
    WebDataManager::RequestResult result = webData.fetchOnce(url, filename);

    lua_pushboolean(L, result.success);
    if (!result.success) {
        lua_pushstring(L, result.error_message.c_str());
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_webdata_fetch_scheduled(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    const char* filename = luaL_checkstring(L, 2);
    int minutes = luaL_checkinteger(L, 3);

    auto& webData = WebDataManager::getInstance();
    bool success = webData.fetchScheduled(url, filename, static_cast<uint32_t>(minutes));

    lua_pushboolean(L, success);
    if (!success) {
        lua_pushstring(L, "Failed to schedule download");
        return 2;
    }
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_webdata_read_data(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);

    auto& webData = WebDataManager::getInstance();
    std::string data = webData.readData(filename);

    if (data.empty()) {
        lua_pushnil(L);
        lua_pushstring(L, "File not found or empty");
        return 2;
    }

    lua_pushstring(L, data.c_str());
    return 1;
}

int VoiceAssistant::LuaSandbox::lua_webdata_list_files(lua_State* L) {
    auto& webData = WebDataManager::getInstance();
    std::vector<std::string> files = webData.listFiles();

    std::ostringstream oss;
    if (files.empty()) {
        oss << "WebData directory is empty";
    } else {
        oss << "WebData files:";
        for (const auto& file : files) {
            oss << "\n- " << file;
        }
    }
    const std::string output = oss.str();
    Serial.println(output.c_str());
    if (s_active_lua_sandbox) {
        s_active_lua_sandbox->appendOutput(output);
    }

    lua_newtable(L);
    for (size_t i = 0; i < files.size(); ++i) {
        lua_pushinteger(L, i + 1);  // Lua arrays are 1-indexed
        lua_pushstring(L, files[i].c_str());
        lua_settable(L, -3);
    }

    return 1;
}

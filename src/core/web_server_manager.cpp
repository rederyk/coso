#include "core/web_server_manager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <cJSON.h>
#include <esp_heap_caps.h>
#include <uri/UriBraces.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/command_center.h"
#include "core/conversation_buffer.h"
#include "core/settings_manager.h"
#include "core/task_config.h"
#include "core/time_scheduler.h"
#include "core/voice_assistant.h"
#include "drivers/sd_card_driver.h"
#include "utils/logger.h"

namespace {
constexpr const char* DEFAULT_ROOT = "/www/index.html";
constexpr uint32_t SERVER_LOOP_DELAY_MS = 10;
constexpr uint32_t ASSISTANT_RESPONSE_TIMEOUT_MS = 50000;  // 50 seconds for first model load
constexpr uint32_t SD_MUTEX_TIMEOUT_MS = 2000;
constexpr size_t MAX_FS_LIST_ENTRIES = 128;

class SdMutexGuard {
public:
    explicit SdMutexGuard(SdCardDriver& driver, uint32_t timeout_ms = SD_MUTEX_TIMEOUT_MS)
        : driver_(driver), locked_(driver_.acquireSdMutex(timeout_ms)) {
    }

    ~SdMutexGuard() {
        if (locked_) {
            driver_.releaseSdMutex();
        }
    }

    bool locked() const { return locked_; }

private:
    SdCardDriver& driver_;
    bool locked_;
};

void sendCalendarJsonResponse(WebServer& server, cJSON* obj, int status_code = 200) {
    if (!obj) {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Internal error\"}");
        return;
    }

    char* json_str = cJSON_PrintUnformatted(obj);
    if (json_str) {
        server.send(status_code, "application/json", json_str);
        free(json_str);
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to build JSON\"}");
    }

    cJSON_Delete(obj);
}

void sendCalendarError(WebServer& server, const char* message, int status_code = 400) {
    cJSON* response = cJSON_CreateObject();
    if (!response) {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Internal error\"}");
        return;
    }

    cJSON_AddBoolToObject(response, "success", false);
    if (message && message[0] != '\0') {
        cJSON_AddStringToObject(response, "error", message);
    }

    sendCalendarJsonResponse(server, response, status_code);
}
}  // namespace

WebServerManager& WebServerManager::getInstance() {
    static WebServerManager instance;
    return instance;
}

WebServerManager::WebServerManager()
    : server_(nullptr),
      task_handle_(nullptr),
      port_(80),
      running_(false),
      routes_registered_(false) {
}

void WebServerManager::start(uint16_t port) {
    if (running_) {
        return;
    }

    port_ = port;
    server_.reset(new WebServer(port_));
    routes_registered_ = false;
    registerRoutes();
    server_->begin();
    running_ = true;

    Logger::getInstance().infof("[WebServer] Started on port %u", static_cast<unsigned>(port));

    xTaskCreatePinnedToCore(
        [](void* ctx) {
            static_cast<WebServerManager*>(ctx)->serverLoop();
        },
        "http_server",
        TaskConfig::STACK_HTTP,
        this,
        TaskConfig::PRIO_HTTP,
        &task_handle_,
        TaskConfig::CORE_WORK);
}

void WebServerManager::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    if (server_) {
        server_->stop();
    }
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    routes_registered_ = false;
    Logger::getInstance().info("[WebServer] Stopped");
}

void WebServerManager::registerRoutes() {
    if (routes_registered_ || !server_) {
        return;
    }

    server_->on("/", HTTP_GET, [this]() { handleRoot(); });
    server_->on("/commands", HTTP_GET, [this]() { handleRoot(); });
    server_->on("/file-manager", HTTP_GET, [this]() { handleFileManagerPage(); });
    server_->on("/lua-console", HTTP_GET, [this]() { handleLuaConsolePage(); });
    server_->on("/api/commands", HTTP_GET, [this]() { handleApiCommands(); });
    server_->on("/api/commands/execute", HTTP_POST, [this]() { handleApiExecute(); });
    server_->on("/api/lua/execute", HTTP_POST, [this]() { handleApiLuaExecute(); });
    server_->on("/api/assistant/chat", HTTP_POST, [this]() { handleAssistantChat(); });
    server_->on("/api/assistant/audio/start", HTTP_POST, [this]() { handleAssistantAudioStart(); });
    server_->on("/api/assistant/audio/stop", HTTP_POST, [this]() { handleAssistantAudioStop(); });
    server_->on("/api/assistant/conversation", HTTP_GET, [this]() { handleAssistantConversationGet(); });
    server_->on("/api/assistant/conversation/reset", HTTP_POST, [this]() { handleAssistantConversationReset(); });
    server_->on("/api/assistant/conversation/limit", HTTP_POST, [this]() { handleAssistantConversationLimit(); });
    server_->on("/api/assistant/settings", HTTP_GET, [this]() { handleAssistantSettingsGet(); });
    server_->on("/api/assistant/settings", HTTP_POST, [this]() { handleAssistantSettingsPost(); });
    server_->on("/api/assistant/prompt", HTTP_GET, [this]() { handleAssistantPromptGet(); });
    server_->on("/api/assistant/prompt", HTTP_POST, [this]() { handleAssistantPromptPost(); });
    server_->on("/api/assistant/prompt/preview", HTTP_POST, [this]() { handleAssistantPromptPreview(); });
    server_->on("/api/assistant/prompt/resolve-and-save", HTTP_POST, [this]() { handleAssistantPromptResolveAndSave(); });
    server_->on("/api/assistant/prompt/variables", HTTP_GET, [this]() { handleAssistantPromptVariables(); });
    server_->on("/api/assistant/models", HTTP_GET, [this]() { handleAssistantModels(); });

    // ========== CALENDAR / SCHEDULER ENDPOINTS ==========
    server_->on("/calendar", HTTP_GET, [this]() { handleCalendarPage(); });
    server_->on("/api/calendar/events", HTTP_GET, [this]() { handleCalendarEventsList(); });
    server_->on("/api/calendar/events", HTTP_POST, [this]() { handleCalendarEventsCreate(); });
    server_->on(UriBraces("/api/calendar/events/{}"), HTTP_DELETE, [this]() { handleCalendarEventsDelete(); });
    server_->on(UriBraces("/api/calendar/events/{}/enable"), HTTP_POST, [this]() { handleCalendarEventsEnable(); });
    server_->on(UriBraces("/api/calendar/events/{}/execute"), HTTP_POST, [this]() { handleCalendarEventsExecute(); });
    server_->on("/api/calendar/settings", HTTP_GET, [this]() { handleCalendarSettingsGet(); });
    server_->on("/api/calendar/settings", HTTP_POST, [this]() { handleCalendarSettingsPost(); });

    // ========== TTS SETTINGS ENDPOINTS ==========
    server_->on("/tts-settings", HTTP_GET, [this]() { handleTtsSettingsPage(); });
    server_->on("/api/tts/settings", HTTP_GET, [this]() { handleTtsSettingsGet(); });
    server_->on("/api/tts/settings", HTTP_POST, [this]() { handleTtsSettingsPost(); });
    server_->on("/api/tts/settings/export", HTTP_GET, [this]() { handleTtsSettingsExport(); });
    server_->on("/api/tts/settings/import", HTTP_POST, [this]() { handleTtsSettingsImport(); });

    server_->on("/api/health", HTTP_GET, [this]() { handleApiHealth(); });
    server_->on("/api/fs/list", HTTP_GET, [this]() { handleFsList(); });
    server_->on("/api/fs/download", HTTP_GET, [this]() { handleFsDownload(); });
    server_->on("/api/fs/mkdir", HTTP_POST, [this]() { handleFsMkdir(); });
    server_->on("/api/fs/rename", HTTP_POST, [this]() { handleFsRename(); });
    server_->on("/api/fs/delete", HTTP_POST, [this]() { handleFsDelete(); });
    server_->on(
        "/api/fs/upload",
        HTTP_POST,
        [this]() { handleFsUploadComplete(); },
        [this]() { handleFsUploadData(); });
    server_->onNotFound([this]() { handleNotFound(); });

    routes_registered_ = true;
}

void WebServerManager::serverLoop() {
    while (running_) {
        if (server_) {
            server_->handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(SERVER_LOOP_DELAY_MS));
    }
}

bool WebServerManager::serveFile(const String& path) {
    if (!LittleFS.exists(path)) {
        return false;
    }
    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }
    const String content_type = contentTypeForPath(path);
    server_->sendHeader("Cache-Control", "no-cache");
    server_->streamFile(file, content_type);
    file.close();
    return true;
}

String WebServerManager::contentTypeForPath(const String& path) const {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".svg")) return "image/svg+xml";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

void WebServerManager::sendJson(int code, const String& payload) {
    server_->sendHeader("Cache-Control", "no-store");
    server_->send(code, "application/json", payload);
}

void WebServerManager::handleRoot() {
    if (!serveFile(DEFAULT_ROOT)) {
        server_->send(404, "text/plain", "index.html not found");
    }
}

void WebServerManager::handleApiCommands() {
    const auto commands = CommandCenter::getInstance().listCommands();
    JsonDocument doc;
    JsonArray arr = doc["commands"].to<JsonArray>();
    for (const auto& cmd : commands) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = cmd.name;
        obj["description"] = cmd.description;
    }

    String response;
    serializeJson(doc, response);
    sendJson(200, response);
}

void WebServerManager::handleApiExecute() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    JsonDocument doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    const char* command = doc["command"] | "";
    if (!command || strlen(command) == 0) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Missing command\"}");
        return;
    }

    std::vector<std::string> args;
    JsonArrayConst arr = doc["args"].as<JsonArrayConst>();
    if (!arr.isNull()) {
        args.reserve(arr.size());
        for (JsonVariantConst v : arr) {
            args.emplace_back(v.as<const char*>() ? v.as<const char*>() : "");
        }
    }

    const CommandResult result = CommandCenter::getInstance().executeCommand(command, args);

    JsonDocument out;
    out["status"] = result.success ? "success" : "error";
    out["command"] = command;
    out["message"] = result.message.c_str();

    String response;
    serializeJson(out, response);
    sendJson(result.success ? 200 : 400, response);
}

void WebServerManager::handleLuaConsolePage() {
    if (!serveFile("/www/lua-console.html")) {
        server_->send(404, "text/plain", "lua-console.html not found");
    }
}

void WebServerManager::handleApiLuaExecute() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    DynamicJsonDocument doc(8192);
    auto err = deserializeJson(doc, body);
    if (err) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    const char* script = doc["script"] | "";
    if (!script || strlen(script) == 0) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Missing script\"}");
        return;
    }

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    const CommandResult result = assistant.executeLuaScript(script);

    StaticJsonDocument<256> out;
    out["status"] = result.success ? "success" : "error";
    out["message"] = result.message.c_str();
    out["script"] = script;

    String response;
    serializeJson(out, response);
    sendJson(result.success ? 200 : 400, response);
}

static void appendCommandToDoc(StaticJsonDocument<512>& doc, const VoiceAssistant::VoiceCommand& response) {
    doc["status"] = "success";
    doc["command"] = response.command.c_str();
    JsonArray args = doc.createNestedArray("args");
    for (const auto& arg : response.args) {
        args.add(arg.c_str());
    }
    doc["text"] = response.text.c_str();
    if (!response.transcription.empty()) {
        doc["transcription"] = response.transcription.c_str();
    }
    if (!response.output.empty()) {
        doc["output"] = response.output.c_str();
    }
}

void WebServerManager::handleAssistantChat() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    StaticJsonDocument<256> request_doc;
    auto err = deserializeJson(request_doc, body);
    if (err) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    const char* message = request_doc["message"] | "";
    if (!message || strlen(message) == 0) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Missing message\"}");
        return;
    }

    SettingsManager& settings = SettingsManager::getInstance();
    if (!settings.getVoiceAssistantEnabled()) {
        sendJson(403, "{\"status\":\"error\",\"message\":\"Voice assistant is disabled\"}");
        return;
    }

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    if (!assistant.isInitialized()) {
        Logger::getInstance().info("[VoiceAssistant] Initializing before web chat request");
        if (!assistant.begin()) {
            Logger::getInstance().warn("[VoiceAssistant] Initialization failed before chat request");
            sendJson(503, "{\"status\":\"error\",\"message\":\"Voice assistant unavailable\"}");
            return;
        }
    }

    if (!assistant.sendTextMessage(message)) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"Voice assistant unavailable\"}");
        return;
    }

    VoiceAssistant::VoiceCommand response;
    if (!assistant.getLastResponse(response, ASSISTANT_RESPONSE_TIMEOUT_MS)) {
        sendJson(504, "{\"status\":\"error\",\"message\":\"No response from assistant\"}");
        return;
    }

    StaticJsonDocument<512> doc;
    appendCommandToDoc(doc, response);
    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebServerManager::handleAssistantAudioStart() {
    SettingsManager& settings = SettingsManager::getInstance();
    VoiceAssistant& assistant = VoiceAssistant::getInstance();

    if (!settings.getVoiceAssistantEnabled()) {
        sendJson(403, "{\"status\":\"error\",\"message\":\"Voice assistant is disabled\"}");
        return;
    }

    if (!assistant.isInitialized()) {
        Logger::getInstance().info("[VoiceAssistant] Initializing before web recording");
        if (!assistant.begin()) {
            Logger::getInstance().warn("[VoiceAssistant] Initialization failed before recording");
            sendJson(503, "{\"status\":\"error\",\"message\":\"Voice assistant unavailable\"}");
            return;
        }
    }

    assistant.startRecording();
    sendJson(200, "{\"status\":\"success\",\"message\":\"Recording started\"}");
}

void WebServerManager::handleAssistantAudioStop() {
    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    assistant.stopRecordingAndProcess();

    VoiceAssistant::VoiceCommand response;
    if (!assistant.getLastResponse(response, ASSISTANT_RESPONSE_TIMEOUT_MS)) {
        sendJson(504, "{\"status\":\"error\",\"message\":\"No response from assistant\"}");
        return;
    }

    StaticJsonDocument<512> doc;
    appendCommandToDoc(doc, response);
    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebServerManager::handleAssistantConversationGet() {
    ConversationBuffer& buffer = ConversationBuffer::getInstance();
    if (!buffer.begin()) {
        sendJson(500, "{\"status\":\"error\",\"message\":\"Conversation buffer unavailable\"}");
        return;
    }

    auto entries = buffer.getEntries();

    JsonDocument doc;
    doc["status"] = "success";
    doc["limit"] = static_cast<uint32_t>(buffer.getLimit());
    doc["size"] = static_cast<uint32_t>(buffer.size());
    JsonArray messages = doc.createNestedArray("messages");
    for (const auto& entry : entries) {
        JsonObject obj = messages.add<JsonObject>();
        obj["role"] = entry.role.c_str();
        obj["text"] = entry.text.c_str();
        obj["timestamp"] = entry.timestamp;
        if (!entry.command.empty()) {
            obj["command"] = entry.command.c_str();
        }
        if (!entry.output.empty()) {
            obj["output"] = entry.output.c_str();
        }
        if (!entry.transcription.empty()) {
            obj["transcription"] = entry.transcription.c_str();
        }
        if (!entry.args.empty()) {
            JsonArray args = obj.createNestedArray("args");
            for (const auto& arg : entry.args) {
                args.add(arg.c_str());
            }
        }
    }

    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebServerManager::handleAssistantConversationReset() {
    if (!ConversationBuffer::getInstance().clear()) {
        sendJson(500, "{\"status\":\"error\",\"message\":\"Impossibile resettare il buffer\"}");
        return;
    }
    sendJson(200, "{\"status\":\"success\",\"message\":\"Buffer conversazione resettato\"}");
}

void WebServerManager::handleAssistantConversationLimit() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    StaticJsonDocument<128> doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    int limit = doc["limit"] | 0;
    if (limit <= 0) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Valore limite non valido\"}");
        return;
    }

    if (!ConversationBuffer::getInstance().setLimit(static_cast<size_t>(limit))) {
        sendJson(500, "{\"status\":\"error\",\"message\":\"Impossibile aggiornare il limite\"}");
        return;
    }

    JsonDocument response;
    response["status"] = "success";
    response["limit"] = static_cast<uint32_t>(ConversationBuffer::getInstance().getLimit());

    String payload;
    serializeJson(response, payload);
    sendJson(200, payload);
}

void WebServerManager::handleAssistantSettingsGet() {
    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    StaticJsonDocument<2048> doc;
    doc["voiceAssistantEnabled"] = snapshot.voiceAssistantEnabled;
    doc["localApiMode"] = snapshot.localApiMode;
    doc["openAiApiKey"] = snapshot.openAiApiKey;
    doc["openAiEndpoint"] = snapshot.openAiEndpoint;
    doc["dockerHostIp"] = snapshot.dockerHostIp;
    doc["whisperCloudEndpoint"] = snapshot.whisperCloudEndpoint;
    doc["whisperLocalEndpoint"] = snapshot.whisperLocalEndpoint;
    doc["llmCloudEndpoint"] = snapshot.llmCloudEndpoint;
    doc["llmLocalEndpoint"] = snapshot.llmLocalEndpoint;
    doc["llmModel"] = snapshot.llmModel;
    doc["activeWhisperEndpoint"] =
        snapshot.localApiMode ? snapshot.whisperLocalEndpoint : snapshot.whisperCloudEndpoint;
    doc["activeLlmEndpoint"] =
        snapshot.localApiMode ? snapshot.llmLocalEndpoint : snapshot.llmCloudEndpoint;
    doc["systemPromptTemplate"] = snapshot.voiceAssistantSystemPromptTemplate;
    doc["systemPrompt"] = VoiceAssistant::getInstance().getSystemPrompt();

    String response;
    serializeJson(doc, response);
    sendJson(200, response);
}

void WebServerManager::handleAssistantSettingsPost() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    StaticJsonDocument<1024> doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    SettingsManager& settings = SettingsManager::getInstance();
    bool previous_voice_assistant_enabled = settings.getVoiceAssistantEnabled();
    bool voice_assistant_settings_updated = false;
    bool requested_voice_assistant_enabled = previous_voice_assistant_enabled;

    if (doc.containsKey("voiceAssistantEnabled")) {
        voice_assistant_settings_updated = true;
        requested_voice_assistant_enabled = doc["voiceAssistantEnabled"] | false;
        settings.setVoiceAssistantEnabled(requested_voice_assistant_enabled);
    }
    if (doc.containsKey("localApiMode")) {
        settings.setLocalApiMode(doc["localApiMode"] | false);
    }

    JsonVariant api_key = doc["openAiApiKey"];
    if (api_key && !api_key.isNull()) {
        settings.setOpenAiApiKey(api_key.as<const char*>());
    }
    JsonVariant api_endpoint = doc["openAiEndpoint"];
    if (api_endpoint && !api_endpoint.isNull()) {
        settings.setOpenAiEndpoint(api_endpoint.as<const char*>());
    }
    JsonVariant docker_ip = doc["dockerHostIp"];
    if (docker_ip && !docker_ip.isNull()) {
        settings.setDockerHostIp(docker_ip.as<const char*>());
    }

    JsonVariant whisper_cloud = doc["whisperCloudEndpoint"];
    if (whisper_cloud && !whisper_cloud.isNull()) {
        settings.setWhisperCloudEndpoint(whisper_cloud.as<const char*>());
    }
    JsonVariant whisper_local = doc["whisperLocalEndpoint"];
    if (whisper_local && !whisper_local.isNull()) {
        settings.setWhisperLocalEndpoint(whisper_local.as<const char*>());
    }

    JsonVariant llm_cloud = doc["llmCloudEndpoint"];
    if (llm_cloud && !llm_cloud.isNull()) {
        settings.setLlmCloudEndpoint(llm_cloud.as<const char*>());
    }
    JsonVariant llm_local = doc["llmLocalEndpoint"];
    if (llm_local && !llm_local.isNull()) {
        settings.setLlmLocalEndpoint(llm_local.as<const char*>());
    }

    JsonVariant llm_model = doc["llmModel"];
    if (llm_model && !llm_model.isNull()) {
        settings.setLlmModel(llm_model.as<const char*>());
    }
    JsonVariant system_prompt = doc["systemPromptTemplate"];
    if (system_prompt && !system_prompt.isNull()) {
        settings.setVoiceAssistantSystemPromptTemplate(system_prompt.as<const char*>());
    }

    if (voice_assistant_settings_updated) {
        VoiceAssistant& assistant = VoiceAssistant::getInstance();
        if (requested_voice_assistant_enabled && !previous_voice_assistant_enabled) {
            Logger::getInstance().info("[VoiceAssistant] Initializing after remote enable");
            if (!assistant.begin()) {
                Logger::getInstance().warn("[VoiceAssistant] Initialization after remote enable failed");
            }
        } else if (!requested_voice_assistant_enabled && previous_voice_assistant_enabled) {
            Logger::getInstance().info("[VoiceAssistant] Deinitializing after remote disable");
            assistant.end();
        }
    }

    sendJson(200, "{\"status\":\"success\",\"message\":\"Impostazioni aggiornate\"}");
}

void WebServerManager::handleAssistantPromptGet() {
    File file = LittleFS.open(VOICE_ASSISTANT_PROMPT_JSON_PATH, FILE_READ);
    if (!file) {
        sendJson(500, "{\"status\":\"error\",\"message\":\"Impossibile leggere il prompt\"}");
        return;
    }

    String payload;
    while (file.available()) {
        payload += file.readString();
    }
    file.close();

    if (payload.isEmpty()) {
        sendJson(500, "{\"status\":\"error\",\"message\":\"Prompt vuoto\"}");
        return;
    }

    sendJson(200, payload);
}

void WebServerManager::handleAssistantPromptPost() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    std::string error;
    if (!assistant.savePromptDefinition(std::string(body.c_str()), error)) {
        StaticJsonDocument<256> response;
        response["status"] = "error";
        response["message"] = error.empty() ? "Salvataggio non riuscito" : error.c_str();
        String payload;
        serializeJson(response, payload);
        sendJson(400, payload);
        return;
    }

    sendJson(200, "{\"status\":\"success\",\"message\":\"Prompt aggiornato\"}");
}

void WebServerManager::handleAssistantPromptPreview() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    std::string error;

    // Execute auto_populate commands to populate variables
    if (!assistant.executeAutoPopulateCommands(std::string(body.c_str()), error)) {
        Logger::getInstance().warnf("[WebServer] Auto-populate failed: %s", error.c_str());
    }

    std::string rendered;
    if (!assistant.buildPromptFromJson(std::string(body.c_str()), error, rendered)) {
        StaticJsonDocument<256> response;
        response["status"] = "error";
        response["message"] = error.empty() ? "Anteprima non disponibile" : error.c_str();
        String payload;
        serializeJson(response, payload);
        sendJson(400, payload);
        return;
    }

    // Get active variables to show in preview
    auto variables = assistant.getSystemPromptVariables();

    DynamicJsonDocument response(4096);
    response["status"] = "success";
    response["resolvedPrompt"] = rendered.c_str();

    // Add variables info
    JsonObject vars = response.createNestedObject("variables");
    for (const auto& pair : variables) {
        std::string value = pair.second;
        if (value.length() > 500) {
            value = value.substr(0, 497) + "...";
        }
        vars[pair.first.c_str()] = value.c_str();
    }

    String payload;
    serializeJson(response, payload);
    sendJson(200, payload);
}

void WebServerManager::handleAssistantPromptVariables() {
    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    auto variables = assistant.getSystemPromptVariables();

    DynamicJsonDocument doc(4096);
    doc["status"] = "success";
    JsonObject vars = doc.createNestedObject("variables");

    for (const auto& pair : variables) {
        // Truncate very long values for display
        std::string value = pair.second;
        if (value.length() > 500) {
            value = value.substr(0, 497) + "...";
        }
        vars[pair.first.c_str()] = value.c_str();
    }

    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebServerManager::handleAssistantPromptResolveAndSave() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    std::string error;
    std::string resolved_json;

    Logger::getInstance().infof("[WebServer] Resolving and saving prompt...");

    // Esegue auto_populate e risolve tutti i placeholder
    if (!assistant.resolveAndSavePrompt(std::string(body.c_str()), error, resolved_json)) {
        Logger::getInstance().warnf("[WebServer] Resolve and save failed: %s", error.c_str());
        StaticJsonDocument<256> response;
        response["status"] = "error";
        response["message"] = error.empty() ? "Risoluzione non riuscita" : error.c_str();
        String payload;
        serializeJson(response, payload);
        sendJson(400, payload);
        return;
    }

    // Ritorna il nuovo JSON risolto
    DynamicJsonDocument response(resolved_json.length() + 512);
    response["status"] = "success";
    response["message"] = "Prompt risolto e salvato";
    response["resolved_json"] = resolved_json.c_str();

    String payload;
    serializeJson(response, payload);
    sendJson(200, payload);

    Logger::getInstance().infof("[WebServer] Prompt resolved and saved successfully");
}

void WebServerManager::handleAssistantModels() {
    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    StaticJsonDocument<2048> doc;
    JsonArray models = doc.createNestedArray("models");
    doc["activeModel"] = snapshot.llmModel;
    doc["mode"] = snapshot.localApiMode ? "local" : "cloud";

    if (snapshot.localApiMode) {
        std::vector<std::string> available;
        if (!VoiceAssistant::getInstance().fetchOllamaModels(snapshot.llmLocalEndpoint, available)) {
            sendJson(502, "{\"status\":\"error\",\"message\":\"Impossibile recuperare i modelli locali\"}");
            return;
        }
        for (const auto& model : available) {
            models.add(model);
        }
    } else {
        constexpr const char* CLOUD_PRESETS[] = {
            "gpt-4",
            "gpt-4-turbo",
            "gpt-4o",
            "gpt-4o-mini",
            "gpt-3.5-turbo"
        };
        for (const char* model : CLOUD_PRESETS) {
            models.add(model);
        }
    }

    String response;
    serializeJson(doc, response);
    sendJson(200, response);
}

void WebServerManager::handleApiHealth() {
    SettingsManager& settings = SettingsManager::getInstance();
    SdCardDriver& sd = SdCardDriver::getInstance();

    JsonDocument doc;
    doc["uptime_ms"] = millis();

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = WiFi.status() == WL_CONNECTED;
    wifi["ip"] = WiFi.localIP().toString();
    wifi["ssid"] = settings.getWifiSsid();

    JsonObject heap = doc["heap"].to<JsonObject>();
    heap["free"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    heap["largest_block"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    heap["psram_free"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    heap["psram_largest"] = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    JsonObject sdj = doc["sd"].to<JsonObject>();
    sdj["mounted"] = sd.isMounted();
    sdj["total"] = sd.totalBytes();
    sdj["used"] = sd.usedBytes();

    String response;
    serializeJson(doc, response);
    sendJson(200, response);
}

void WebServerManager::handleFileManagerPage() {
    if (!serveFile("/www/file-manager.html")) {
        server_->send(404, "text/plain", "file-manager.html not found");
    }
}

void WebServerManager::handleFsList() {
    SdCardDriver& sd = SdCardDriver::getInstance();
    if (!sd.begin()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card unavailable\"}");
        return;
    }

    const String requested_path = server_->hasArg("path") ? server_->arg("path") : "/";
    std::string normalized_path;
    if (!normalizeSdPath(requested_path, normalized_path)) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid path\"}");
        return;
    }

    size_t limit = MAX_FS_LIST_ENTRIES;
    if (server_->hasArg("limit")) {
        int raw_limit = server_->arg("limit").toInt();
        if (raw_limit > 0) {
            limit = static_cast<size_t>(std::min<int>(raw_limit, MAX_FS_LIST_ENTRIES));
        }
    }

    SdMutexGuard guard(sd);
    if (!guard.locked()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card busy\"}");
        return;
    }

    File dir = SD_MMC.open(normalized_path.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        sendJson(404, "{\"status\":\"error\",\"message\":\"Directory not found\"}");
        return;
    }
    dir.close();

    auto entries = sd.listDirectory(normalized_path.c_str(), limit);

    JsonDocument doc;
    doc["status"] = "success";
    doc["path"] = normalized_path.c_str();
    doc["parent"] = parentPath(normalized_path).c_str();
    doc["limit"] = static_cast<uint32_t>(limit);
    doc["count"] = static_cast<uint32_t>(entries.size());
    JsonArray arr = doc.createNestedArray("entries");
    for (const auto& entry : entries) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = entry.name.c_str();
        obj["directory"] = entry.isDirectory;
        if (!entry.isDirectory) {
            obj["size"] = static_cast<uint64_t>(entry.sizeBytes);
        }
    }
    JsonObject storage = doc["storage"].to<JsonObject>();
    storage["mounted"] = sd.isMounted();
    storage["used"] = sd.usedBytes();
    storage["total"] = sd.totalBytes();

    String payload;
    serializeJson(doc, payload);
    sendJson(200, payload);
}

void WebServerManager::handleFsDownload() {
    SdCardDriver& sd = SdCardDriver::getInstance();
    if (!sd.begin()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card unavailable\"}");
        return;
    }
    if (!server_->hasArg("path")) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Missing path\"}");
        return;
    }
    std::string normalized_path;
    if (!normalizeSdPath(server_->arg("path"), normalized_path) || normalized_path == "/") {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid file path\"}");
        return;
    }

    SdMutexGuard guard(sd);
    if (!guard.locked()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card busy\"}");
        return;
    }

    File file = SD_MMC.open(normalized_path.c_str(), FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        sendJson(404, "{\"status\":\"error\",\"message\":\"File not found\"}");
        return;
    }

    String filename = safeBasename(normalized_path);
    server_->sendHeader("Cache-Control", "no-store");
    server_->sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server_->streamFile(file, "application/octet-stream");
    file.close();
}

void WebServerManager::handleFsMkdir() {
    SdCardDriver& sd = SdCardDriver::getInstance();
    if (!sd.begin()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card unavailable\"}");
        return;
    }

    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    const char* parent_raw = doc["path"] | "/";
    const char* name_raw = doc["name"] | "";
    std::string parent_path;
    std::string folder_name;
    if (!normalizeSdPath(parent_raw, parent_path) || !sanitizeFilename(name_raw, folder_name)) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid directory name\"}");
        return;
    }

    SdMutexGuard guard(sd);
    if (!guard.locked()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card busy\"}");
        return;
    }

    File parent = SD_MMC.open(parent_path.c_str());
    if (!parent || !parent.isDirectory()) {
        if (parent) {
            parent.close();
        }
        sendJson(404, "{\"status\":\"error\",\"message\":\"Parent directory not found\"}");
        return;
    }
    parent.close();

    std::string full_path = joinPaths(parent_path, folder_name);
    if (full_path == "/" || full_path.size() > 255) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid target path\"}");
        return;
    }

    if (SD_MMC.exists(full_path.c_str())) {
        sendJson(409, "{\"status\":\"error\",\"message\":\"Entry already exists\"}");
        return;
    }

    if (!SD_MMC.mkdir(full_path.c_str())) {
        sendJson(500, "{\"status\":\"error\",\"message\":\"Failed to create directory\"}");
        return;
    }

    sd.refreshStats();
    sendJson(200, "{\"status\":\"success\",\"message\":\"Directory created\"}");
}

void WebServerManager::handleFsRename() {
    SdCardDriver& sd = SdCardDriver::getInstance();
    if (!sd.begin()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card unavailable\"}");
        return;
    }

    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    const char* from_raw = doc["from"] | "";
    const char* to_raw = doc["to"] | "";

    std::string from_path;
    std::string to_path;
    if (!normalizeSdPath(from_raw, from_path) || !normalizeSdPath(to_raw, to_path) ||
        to_path == "/") {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid rename paths\"}");
        return;
    }

    SdMutexGuard guard(sd);
    if (!guard.locked()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card busy\"}");
        return;
    }

    if (!SD_MMC.exists(from_path.c_str())) {
        sendJson(404, "{\"status\":\"error\",\"message\":\"Source path not found\"}");
        return;
    }

    if (SD_MMC.exists(to_path.c_str())) {
        sendJson(409, "{\"status\":\"error\",\"message\":\"Destination already exists\"}");
        return;
    }

    if (!SD_MMC.rename(from_path.c_str(), to_path.c_str())) {
        sendJson(500, "{\"status\":\"error\",\"message\":\"Rename failed\"}");
        return;
    }

    sd.refreshStats();
    JsonDocument response;
    response["status"] = "success";
    response["from"] = from_path.c_str();
    response["to"] = to_path.c_str();

    String payload;
    serializeJson(response, payload);
    sendJson(200, payload);
}

void WebServerManager::handleFsDelete() {
    SdCardDriver& sd = SdCardDriver::getInstance();
    if (!sd.begin()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card unavailable\"}");
        return;
    }

    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    StaticJsonDocument<192> doc;
    if (deserializeJson(doc, body)) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    const char* path_raw = doc["path"] | "";
    std::string path;
    if (!normalizeSdPath(path_raw, path) || path == "/") {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Invalid path\"}");
        return;
    }

    SdMutexGuard guard(sd);
    if (!guard.locked()) {
        sendJson(503, "{\"status\":\"error\",\"message\":\"SD card busy\"}");
        return;
    }

    if (!SD_MMC.exists(path.c_str())) {
        sendJson(404, "{\"status\":\"error\",\"message\":\"Path not found\"}");
        return;
    }

    if (!sd.removePath(path.c_str())) {
        std::string message = sd.lastError();
        if (message.empty()) {
            message = "Delete failed";
        }
        JsonDocument error;
        error["status"] = "error";
        error["message"] = message.c_str();
        String payload;
        serializeJson(error, payload);
        sendJson(500, payload);
        return;
    }

    JsonDocument response;
    response["status"] = "success";
    response["path"] = path.c_str();

    String payload;
    serializeJson(response, payload);
    sendJson(200, payload);
}

void WebServerManager::handleFsUploadData() {
    HTTPUpload& upload = server_->upload();
    SdCardDriver& sd = SdCardDriver::getInstance();

    auto fail = [&](const char* message) {
        upload_state_.error = true;
        upload_state_.message = message;
        if (upload_state_.file) {
            upload_state_.file.close();
        }
        if (upload_state_.lock_acquired) {
            sd.releaseSdMutex();
            upload_state_.lock_acquired = false;
        }
    };

    switch (upload.status) {
    case UPLOAD_FILE_START: {
        upload_state_ = UploadState{};
        upload_state_.active = true;
        upload_state_.bytes_written = 0;

        if (!sd.begin()) {
            fail("SD card unavailable");
            return;
        }

        std::string base_path;
        const String base_arg = server_->hasArg("path") ? server_->arg("path") : "/";
        if (!normalizeSdPath(base_arg, base_path)) {
            fail("Invalid target path");
            return;
        }

        std::string filename;
        if (!sanitizeFilename(upload.filename, filename)) {
            fail("Invalid file name");
            return;
        }

        if (!sd.acquireSdMutex(SD_MUTEX_TIMEOUT_MS)) {
            fail("SD card busy");
            return;
        }
        upload_state_.lock_acquired = true;

        File parent = SD_MMC.open(base_path.c_str());
        if (!parent || !parent.isDirectory()) {
            if (parent) {
                parent.close();
            }
            fail("Target directory missing");
            return;
        }
        parent.close();

        std::string full_path = joinPaths(base_path, filename);
        if (full_path.size() > 255) {
            fail("Path too long");
            return;
        }

        if (SD_MMC.exists(full_path.c_str())) {
            if (!SD_MMC.remove(full_path.c_str())) {
                fail("Cannot overwrite file");
                return;
            }
        }

        upload_state_.file = SD_MMC.open(full_path.c_str(), FILE_WRITE);
        if (!upload_state_.file) {
            fail("Open file failed");
            return;
        }
        upload_state_.path = full_path;
        upload_state_.message = "Receiving file";
        break;
    }
    case UPLOAD_FILE_WRITE:
        if (!upload_state_.error && upload_state_.file && upload.currentSize) {
            size_t written = upload_state_.file.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize) {
                fail("Write error");
            } else {
                upload_state_.bytes_written += written;
            }
        }
        break;
    case UPLOAD_FILE_END:
        if (!upload_state_.error && upload_state_.file) {
            upload_state_.file.flush();
            upload_state_.file.close();
            upload_state_.message = "Upload completed";
            upload_state_.completed = true;
            sd.refreshStats();
        }
        if (upload_state_.lock_acquired) {
            sd.releaseSdMutex();
            upload_state_.lock_acquired = false;
        }
        break;
    case UPLOAD_FILE_ABORTED:
        fail("Upload aborted");
        break;
    default:
        break;
    }
}

void WebServerManager::handleFsUploadComplete() {
    JsonDocument doc;
    if (!upload_state_.active) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"No upload in progress\"}");
        return;
    }

    if (upload_state_.lock_acquired) {
        SdCardDriver::getInstance().releaseSdMutex();
        upload_state_.lock_acquired = false;
    }

    doc["status"] = upload_state_.error ? "error" : "success";
    doc["message"] = upload_state_.message.length() ? upload_state_.message.c_str()
                                                    : (upload_state_.error ? "Upload failed"
                                                                           : "Upload completed");
    if (!upload_state_.path.empty()) {
        doc["path"] = upload_state_.path.c_str();
    }
    doc["bytes"] = static_cast<uint32_t>(upload_state_.bytes_written);

    String payload;
    serializeJson(doc, payload);
    sendJson(upload_state_.error ? 400 : 200, payload);

    if (upload_state_.file) {
        upload_state_.file.close();
    }
    upload_state_ = UploadState{};
}

void WebServerManager::handleNotFound() {
    const String path = server_->uri();
    String static_path = path;
    if (!static_path.startsWith("/www/")) {
        static_path = "/www" + path;
    }

    if (serveFile(static_path)) {
        return;
    }

    Logger::getInstance().warnf("[WebServer] 404 %s", path.c_str());
    server_->send(404, "application/json", "{\"status\":\"error\",\"message\":\"Not found\"}");
}

bool WebServerManager::normalizeSdPath(const String& raw, std::string& out) const {
    const std::string input = raw.c_str();
    if (input.empty()) {
        out = "/";
        return true;
    }

    std::vector<std::string> segments;
    size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && (input[i] == '/' || input[i] == '\\')) {
            ++i;
        }
        size_t start = i;
        while (i < input.size() && input[i] != '/' && input[i] != '\\') {
            char ch = input[i];
            if (ch < 32 || ch == ':' || ch == '*' || ch == '?' || ch == '|' || ch == '"' ||
                ch == '<' || ch == '>' || ch == '\r' || ch == '\n') {
                return false;
            }
            ++i;
        }
        if (start == i) {
            continue;
        }
        std::string segment = input.substr(start, i - start);
        if (segment == ".") {
            continue;
        }
        if (segment == "..") {
            if (segments.empty()) {
                return false;
            }
            segments.pop_back();
            continue;
        }
        segments.push_back(segment);
    }

    out.clear();
    out.push_back('/');
    for (size_t idx = 0; idx < segments.size(); ++idx) {
        out += segments[idx];
        if (idx + 1 < segments.size()) {
            out.push_back('/');
        }
    }
    if (out.size() > 1 && out.back() == '/') {
        out.pop_back();
    }
    if (out.empty()) {
        out = "/";
    }
    if (out.size() > 255) {
        return false;
    }
    return true;
}

bool WebServerManager::sanitizeFilename(const String& raw, std::string& out) const {
    std::string name = raw.c_str();
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
        name = name.substr(slash + 1);
    }
    if (name.empty() || name.size() > 96) {
        return false;
    }
    size_t start = 0;
    while (start < name.size() && name[start] == ' ') {
        ++start;
    }
    size_t end = name.size();
    while (end > start && name[end - 1] == ' ') {
        --end;
    }
    name = name.substr(start, end - start);
    if (name.empty()) {
        return false;
    }
    for (char& ch : name) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (!(std::isalnum(c) || ch == '.' || ch == '_' || ch == '-' || ch == ' ')) {
            return false;
        }
    }
    out = name;
    return true;
}

std::string WebServerManager::joinPaths(const std::string& parent, const std::string& child) const {
    if (child.empty()) {
        return parent.empty() ? std::string("/") : parent;
    }
    if (parent.empty() || parent == "/") {
        return "/" + child;
    }
    std::string result = parent;
    if (result.back() != '/') {
        result.push_back('/');
    }
    result += child;
    return result;
}

std::string WebServerManager::parentPath(const std::string& path) const {
    if (path.empty() || path == "/") {
        return "/";
    }
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

String WebServerManager::safeBasename(const std::string& path) const {
    if (path.empty()) {
        return "download.bin";
    }
    std::string name;
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        name = path;
    } else if (pos + 1 < path.size()) {
        name = path.substr(pos + 1);
    }
    if (name.empty()) {
        name = "download.bin";
    }
    for (char& ch : name) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c < 32 || ch == '"' || ch == '\\') {
            ch = '_';
        }
    }
    return String(name.c_str());
}

void WebServerManager::handleCalendarPage() {
    if (!serveFile("/www/calendar.html")) {
        server_->send(404, "text/plain", "calendar.html not found");
    }
}

void WebServerManager::handleCalendarEventsList() {
    auto& scheduler = TimeScheduler::getInstance();
    auto events = scheduler.listEvents();

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        sendCalendarError(*server_, "Failed to allocate response", 500);
        return;
    }

    cJSON_AddBoolToObject(root, "success", true);
    cJSON* events_array = cJSON_CreateArray();
    if (!events_array) {
        cJSON_Delete(root);
        sendCalendarError(*server_, "Failed to allocate events array", 500);
        return;
    }

    for (const auto& event : events) {
        cJSON* event_obj = cJSON_CreateObject();
        if (!event_obj) {
            cJSON_Delete(events_array);
            cJSON_Delete(root);
            sendCalendarError(*server_, "Failed to build event entry", 500);
            return;
        }

        cJSON_AddStringToObject(event_obj, "id", event.id.c_str());
        cJSON_AddStringToObject(event_obj, "name", event.name.c_str());
        cJSON_AddStringToObject(event_obj, "description", event.description.c_str());
        cJSON_AddNumberToObject(event_obj, "type", static_cast<int>(event.type));
        cJSON_AddBoolToObject(event_obj, "enabled", event.enabled);
        cJSON_AddNumberToObject(event_obj, "hour", event.hour);
        cJSON_AddNumberToObject(event_obj, "minute", event.minute);
        cJSON_AddNumberToObject(event_obj, "weekdays", event.weekdays);
        cJSON_AddNumberToObject(event_obj, "year", event.year);
        cJSON_AddNumberToObject(event_obj, "month", event.month);
        cJSON_AddNumberToObject(event_obj, "day", event.day);
        cJSON_AddStringToObject(event_obj, "lua_script", event.lua_script.c_str());
        cJSON_AddNumberToObject(event_obj, "created_at", event.created_at);
        cJSON_AddNumberToObject(event_obj, "last_run", event.last_run);
        cJSON_AddNumberToObject(event_obj, "next_run", event.next_run);
        cJSON_AddNumberToObject(event_obj, "execution_count", event.execution_count);
        cJSON_AddNumberToObject(event_obj, "status", static_cast<int>(event.status));
        cJSON_AddStringToObject(event_obj, "last_error", event.last_error.c_str());

        cJSON_AddItemToArray(events_array, event_obj);
    }

    cJSON_AddItemToObject(root, "events", events_array);
    sendCalendarJsonResponse(*server_, root);
}

void WebServerManager::handleCalendarEventsCreate() {
    if (!server_->hasArg("plain")) {
        sendCalendarError(*server_, "Missing request body");
        return;
    }

    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendCalendarError(*server_, "Missing request body");
        return;
    }

    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        sendCalendarError(*server_, "Invalid JSON");
        return;
    }

    TimeScheduler::CalendarEvent event{};

    cJSON* name = cJSON_GetObjectItem(json, "name");
    cJSON* type = cJSON_GetObjectItem(json, "type");
    cJSON* hour = cJSON_GetObjectItem(json, "hour");
    cJSON* minute = cJSON_GetObjectItem(json, "minute");
    cJSON* lua = cJSON_GetObjectItem(json, "lua_script");

    if (!name || !cJSON_IsString(name) || !name->valuestring || !type || !cJSON_IsNumber(type) ||
        !hour || !cJSON_IsNumber(hour) || !minute || !cJSON_IsNumber(minute) || !lua ||
        !cJSON_IsString(lua) || !lua->valuestring) {
        cJSON_Delete(json);
        sendCalendarError(*server_, "Missing mandatory fields");
        return;
    }

    event.name = name->valuestring;
    cJSON* desc = cJSON_GetObjectItem(json, "description");
    if (desc && cJSON_IsString(desc) && desc->valuestring) {
        event.description = desc->valuestring;
    }

    int type_value = type->valueint;
    event.type = (type_value == 0) ? TimeScheduler::EventType::OneShot
                                   : TimeScheduler::EventType::Recurring;

    cJSON* enabled_item = cJSON_GetObjectItem(json, "enabled");
    event.enabled = enabled_item ? cJSON_IsTrue(enabled_item) : true;
    event.hour = static_cast<uint8_t>(hour->valueint);
    event.minute = static_cast<uint8_t>(minute->valueint);

    cJSON* weekdays = cJSON_GetObjectItem(json, "weekdays");
    event.weekdays = weekdays && cJSON_IsNumber(weekdays) ? static_cast<uint8_t>(weekdays->valueint) : 0;

    cJSON* year = cJSON_GetObjectItem(json, "year");
    event.year = year && cJSON_IsNumber(year) ? static_cast<uint16_t>(year->valueint) : 0;

    cJSON* month = cJSON_GetObjectItem(json, "month");
    event.month = month && cJSON_IsNumber(month) ? static_cast<uint8_t>(month->valueint) : 0;

    cJSON* day = cJSON_GetObjectItem(json, "day");
    event.day = day && cJSON_IsNumber(day) ? static_cast<uint8_t>(day->valueint) : 0;

    event.lua_script = lua->valuestring;

    cJSON_Delete(json);

    auto& scheduler = TimeScheduler::getInstance();
    std::string id = scheduler.createEvent(event);
    if (id.empty()) {
        sendCalendarError(*server_, "Failed to create event", 500);
        return;
    }

    cJSON* response = cJSON_CreateObject();
    if (!response) {
        sendCalendarError(*server_, "Failed to allocate response", 500);
        return;
    }
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "id", id.c_str());
    sendCalendarJsonResponse(*server_, response);
}

void WebServerManager::handleCalendarEventsDelete() {
    String event_id = server_->pathArg(0);
    if (event_id.isEmpty()) {
        sendCalendarError(*server_, "Missing event ID");
        return;
    }

    auto& scheduler = TimeScheduler::getInstance();
    bool success = scheduler.deleteEvent(event_id.c_str());

    cJSON* response = cJSON_CreateObject();
    if (!response) {
        sendCalendarError(*server_, "Failed to allocate response", 500);
        return;
    }

    cJSON_AddBoolToObject(response, "success", success);
    if (!success) {
        cJSON_AddStringToObject(response, "error", "Event not found");
    }

    sendCalendarJsonResponse(*server_, response);
}

void WebServerManager::handleCalendarEventsEnable() {
    String event_id = server_->pathArg(0);
    if (event_id.isEmpty()) {
        sendCalendarError(*server_, "Missing event ID");
        return;
    }

    if (!server_->hasArg("plain")) {
        sendCalendarError(*server_, "Missing request body");
        return;
    }

    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendCalendarError(*server_, "Missing request body");
        return;
    }

    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        sendCalendarError(*server_, "Invalid JSON");
        return;
    }

    cJSON* enabled_item = cJSON_GetObjectItem(json, "enabled");
    if (!enabled_item || !(cJSON_IsBool(enabled_item) || cJSON_IsNumber(enabled_item))) {
        cJSON_Delete(json);
        sendCalendarError(*server_, "Missing 'enabled' flag");
        return;
    }

    bool enabled = cJSON_IsBool(enabled_item) ? cJSON_IsTrue(enabled_item)
                                              : (enabled_item->valueint != 0);
    cJSON_Delete(json);

    auto& scheduler = TimeScheduler::getInstance();
    bool success = scheduler.enableEvent(event_id.c_str(), enabled);

    cJSON* response = cJSON_CreateObject();
    if (!response) {
        sendCalendarError(*server_, "Failed to allocate response", 500);
        return;
    }

    cJSON_AddBoolToObject(response, "success", success);
    if (!success) {
        cJSON_AddStringToObject(response, "error", "Event not found");
    }

    sendCalendarJsonResponse(*server_, response);
}

void WebServerManager::handleCalendarEventsExecute() {
    String event_id = server_->pathArg(0);
    if (event_id.isEmpty()) {
        sendCalendarError(*server_, "Missing event ID");
        return;
    }

    auto& scheduler = TimeScheduler::getInstance();
    bool success = scheduler.executeEventNow(event_id.c_str());

    cJSON* response = cJSON_CreateObject();
    if (!response) {
        sendCalendarError(*server_, "Failed to allocate response", 500);
        return;
    }

    cJSON_AddBoolToObject(response, "success", success);
    if (!success) {
        cJSON_AddStringToObject(response, "error", "Event not found or execution failed");
    }

    sendCalendarJsonResponse(*server_, response);
}

void WebServerManager::handleCalendarSettingsGet() {
    auto& scheduler = TimeScheduler::getInstance();

    cJSON* response = cJSON_CreateObject();
    if (!response) {
        sendCalendarError(*server_, "Failed to allocate response", 500);
        return;
    }

    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddBoolToObject(response, "enabled", scheduler.isEnabled());

    sendCalendarJsonResponse(*server_, response);
}

void WebServerManager::handleCalendarSettingsPost() {
    if (!server_->hasArg("plain")) {
        sendCalendarError(*server_, "Missing request body");
        return;
    }

    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendCalendarError(*server_, "Missing request body");
        return;
    }

    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        sendCalendarError(*server_, "Invalid JSON");
        return;
    }

    cJSON* enabled_item = cJSON_GetObjectItem(json, "enabled");
    if (!enabled_item || !(cJSON_IsBool(enabled_item) || cJSON_IsNumber(enabled_item))) {
        cJSON_Delete(json);
        sendCalendarError(*server_, "Missing 'enabled' flag");
        return;
    }

    bool enabled = cJSON_IsBool(enabled_item) ? cJSON_IsTrue(enabled_item)
                                              : (enabled_item->valueint != 0);
    cJSON_Delete(json);

    auto& scheduler = TimeScheduler::getInstance();
    scheduler.setEnabled(enabled);

    cJSON* response = cJSON_CreateObject();
    if (!response) {
        sendCalendarError(*server_, "Failed to allocate response", 500);
        return;
    }

    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddBoolToObject(response, "enabled", enabled);

    sendCalendarJsonResponse(*server_, response);
}

// ========== TTS SETTINGS HANDLERS ==========

void WebServerManager::handleTtsSettingsPage() {
    if (!serveFile("/www/tts-settings.html")) {
        server_->send(404, "text/plain", "tts-settings.html not found");
    }
}

void WebServerManager::handleTtsSettingsGet() {
    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    DynamicJsonDocument doc(2048);
    doc["success"] = true;

    // TTS settings
    doc["ttsEnabled"] = snapshot.ttsEnabled;
    doc["ttsCloudEndpoint"] = snapshot.ttsCloudEndpoint;
    doc["ttsLocalEndpoint"] = snapshot.ttsLocalEndpoint;
    doc["ttsVoice"] = snapshot.ttsVoice;
    doc["ttsModel"] = snapshot.ttsModel;
    doc["ttsSpeed"] = snapshot.ttsSpeed;
    doc["ttsOutputFormat"] = snapshot.ttsOutputFormat;
    doc["ttsOutputPath"] = snapshot.ttsOutputPath;

    // Include mode info
    doc["localApiMode"] = snapshot.localApiMode;
    doc["activeTtsEndpoint"] = snapshot.localApiMode ? snapshot.ttsLocalEndpoint : snapshot.ttsCloudEndpoint;

    String response;
    serializeJson(doc, response);
    sendJson(200, response);
}

void WebServerManager::handleTtsSettingsPost() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"success\":false,\"message\":\"Empty body\"}");
        return;
    }

    DynamicJsonDocument doc(2048);
    auto err = deserializeJson(doc, body);
    if (err) {
        sendJson(400, "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }

    SettingsManager& settings = SettingsManager::getInstance();

    // Update TTS settings
    if (doc.containsKey("ttsEnabled")) {
        settings.setTtsEnabled(doc["ttsEnabled"] | false);
    }
    if (doc.containsKey("ttsCloudEndpoint")) {
        JsonVariant val = doc["ttsCloudEndpoint"];
        if (!val.isNull()) {
            settings.setTtsCloudEndpoint(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsLocalEndpoint")) {
        JsonVariant val = doc["ttsLocalEndpoint"];
        if (!val.isNull()) {
            settings.setTtsLocalEndpoint(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsVoice")) {
        JsonVariant val = doc["ttsVoice"];
        if (!val.isNull()) {
            settings.setTtsVoice(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsModel")) {
        JsonVariant val = doc["ttsModel"];
        if (!val.isNull()) {
            settings.setTtsModel(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsSpeed")) {
        settings.setTtsSpeed(doc["ttsSpeed"] | 1.0f);
    }
    if (doc.containsKey("ttsOutputFormat")) {
        JsonVariant val = doc["ttsOutputFormat"];
        if (!val.isNull()) {
            settings.setTtsOutputFormat(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsOutputPath")) {
        JsonVariant val = doc["ttsOutputPath"];
        if (!val.isNull()) {
            settings.setTtsOutputPath(val.as<const char*>());
        }
    }

    sendJson(200, "{\"success\":true,\"message\":\"TTS settings updated\"}");
}

void WebServerManager::handleTtsSettingsExport() {
    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    DynamicJsonDocument doc(2048);
    doc["ttsEnabled"] = snapshot.ttsEnabled;
    doc["ttsCloudEndpoint"] = snapshot.ttsCloudEndpoint;
    doc["ttsLocalEndpoint"] = snapshot.ttsLocalEndpoint;
    doc["ttsVoice"] = snapshot.ttsVoice;
    doc["ttsModel"] = snapshot.ttsModel;
    doc["ttsSpeed"] = snapshot.ttsSpeed;
    doc["ttsOutputFormat"] = snapshot.ttsOutputFormat;
    doc["ttsOutputPath"] = snapshot.ttsOutputPath;

    String response;
    serializeJson(doc, response);

    server_->sendHeader("Content-Disposition", "attachment; filename=\"tts-settings.json\"");
    server_->sendHeader("Cache-Control", "no-store");
    sendJson(200, response);
}

void WebServerManager::handleTtsSettingsImport() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"success\":false,\"message\":\"Empty body\"}");
        return;
    }

    DynamicJsonDocument doc(2048);
    auto err = deserializeJson(doc, body);
    if (err) {
        sendJson(400, "{\"success\":false,\"message\":\"Invalid JSON format\"}");
        return;
    }

    SettingsManager& settings = SettingsManager::getInstance();

    // Import all TTS settings from JSON
    if (doc.containsKey("ttsEnabled")) {
        settings.setTtsEnabled(doc["ttsEnabled"] | false);
    }
    if (doc.containsKey("ttsCloudEndpoint")) {
        JsonVariant val = doc["ttsCloudEndpoint"];
        if (!val.isNull()) {
            settings.setTtsCloudEndpoint(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsLocalEndpoint")) {
        JsonVariant val = doc["ttsLocalEndpoint"];
        if (!val.isNull()) {
            settings.setTtsLocalEndpoint(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsVoice")) {
        JsonVariant val = doc["ttsVoice"];
        if (!val.isNull()) {
            settings.setTtsVoice(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsModel")) {
        JsonVariant val = doc["ttsModel"];
        if (!val.isNull()) {
            settings.setTtsModel(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsSpeed")) {
        settings.setTtsSpeed(doc["ttsSpeed"] | 1.0f);
    }
    if (doc.containsKey("ttsOutputFormat")) {
        JsonVariant val = doc["ttsOutputFormat"];
        if (!val.isNull()) {
            settings.setTtsOutputFormat(val.as<const char*>());
        }
    }
    if (doc.containsKey("ttsOutputPath")) {
        JsonVariant val = doc["ttsOutputPath"];
        if (!val.isNull()) {
            settings.setTtsOutputPath(val.as<const char*>());
        }
    }

    sendJson(200, "{\"success\":true,\"message\":\"TTS settings imported successfully\"}");
}

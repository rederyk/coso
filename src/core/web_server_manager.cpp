#include "core/web_server_manager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <vector>

#include "core/command_center.h"
#include "core/settings_manager.h"
#include "core/task_config.h"
#include "drivers/sd_card_driver.h"
#include "utils/logger.h"
#include "core/voice_assistant.h"

namespace {
constexpr const char* DEFAULT_ROOT = "/www/index.html";
constexpr uint32_t SERVER_LOOP_DELAY_MS = 10;
constexpr uint32_t ASSISTANT_RESPONSE_TIMEOUT_MS = 20000;
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
    server_->on("/api/commands", HTTP_GET, [this]() { handleApiCommands(); });
    server_->on("/api/commands/execute", HTTP_POST, [this]() { handleApiExecute(); });
    server_->on("/api/assistant/chat", HTTP_POST, [this]() { handleAssistantChat(); });
    server_->on("/api/assistant/audio/start", HTTP_POST, [this]() { handleAssistantAudioStart(); });
    server_->on("/api/assistant/audio/stop", HTTP_POST, [this]() { handleAssistantAudioStop(); });
    server_->on("/api/assistant/settings", HTTP_GET, [this]() { handleAssistantSettingsGet(); });
    server_->on("/api/assistant/settings", HTTP_POST, [this]() { handleAssistantSettingsPost(); });
    server_->on("/api/assistant/models", HTTP_GET, [this]() { handleAssistantModels(); });
    server_->on("/api/health", HTTP_GET, [this]() { handleApiHealth(); });
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

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
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

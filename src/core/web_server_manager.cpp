#include "core/web_server_manager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "core/command_center.h"
#include "core/settings_manager.h"
#include "core/task_config.h"
#include "drivers/sd_card_driver.h"
#include "utils/logger.h"

namespace {
constexpr const char* DEFAULT_ROOT = "/www/commands.html";
constexpr uint32_t SERVER_LOOP_DELAY_MS = 10;
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
        server_->send(404, "text/plain", "commands.html not found");
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

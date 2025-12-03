#pragma once

#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <memory>
#include <string>

class WebServerManager {
public:
    static WebServerManager& getInstance();

    void start(uint16_t port = 80);
    void stop();
    bool isRunning() const { return running_; }

private:
    WebServerManager();

    void registerRoutes();
    void serverLoop();
    bool serveFile(const String& path);
    String contentTypeForPath(const String& path) const;
    void sendJson(int code, const String& payload);

    void handleRoot();
    void handleApiCommands();
    void handleApiExecute();
    void handleAssistantChat();
    void handleAssistantAudioStart();
    void handleAssistantAudioStop();
    void handleApiHealth();
    void handleNotFound();

    std::unique_ptr<WebServer> server_;
    TaskHandle_t task_handle_;
    uint16_t port_;
    bool running_;
    bool routes_registered_;
};

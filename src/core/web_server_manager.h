#pragma once

#include <FS.h>
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
    void handleLuaConsolePage();
    void handleApiLuaExecute();
    void handleAssistantChat();
    void handleAssistantChatStatus();
    void handleAssistantAudioStart();
    void handleAssistantAudioStop();
    void handleAssistantConversationGet();
    void handleAssistantConversationReset();
    void handleAssistantConversationLimit();
    void handleAssistantSettingsGet();
    void handleAssistantSettingsPost();
    void handleAssistantPromptGet();
    void handleAssistantPromptPost();
    void handleAssistantPromptPreview();
    void handleAssistantPromptResolveAndSave();
    void handleAssistantPromptVariables();
    void handleAssistantModels();
    void handleApiHealth();
    void handleFileManagerPage();
    void handleFsList();
    void handleFsDownload();
    void handleFsMkdir();
    void handleFsRename();
    void handleFsDelete();
    void handleFsUploadData();
    void handleFsUploadComplete();
    void handleNotFound();

    // Calendar / Scheduler handlers
    void handleCalendarPage();
    void handleCalendarEventsList();
    void handleCalendarEventsCreate();
    void handleCalendarEventsDelete();
    void handleCalendarEventsEnable();
    void handleCalendarEventsExecute();
    void handleCalendarSettingsGet();
    void handleCalendarSettingsPost();

    // TTS Settings handlers
    void handleTtsSettingsPage();
    void handleTtsSettingsGet();
    void handleTtsSettingsPost();
    void handleTtsSettingsExport();
    void handleTtsSettingsImport();
    void handleTtsOptions();

    bool normalizeSdPath(const String& raw, std::string& out) const;
    bool sanitizeFilename(const String& raw, std::string& out) const;
    std::string joinPaths(const std::string& parent, const std::string& child) const;
    std::string parentPath(const std::string& path) const;
    String safeBasename(const std::string& path) const;

    std::unique_ptr<WebServer> server_;
    TaskHandle_t task_handle_;
    uint16_t port_;
    bool running_;
    bool routes_registered_;

    struct UploadState {
        fs::File file;
        std::string path;
        bool active = false;
        bool error = false;
        bool lock_acquired = false;
        bool completed = false;
        size_t bytes_written = 0;
        String message;
    } upload_state_;
};

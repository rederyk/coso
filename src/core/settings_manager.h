#pragma once

#include "core/theme_palette.h"
#include "core/voice_assistant_prompt.h"
#include "core/operating_modes.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <sdkconfig.h>

struct SettingsSnapshot {
    // WiFi & Network
    std::string wifiSsid;
    std::string wifiPassword;
    bool wifiAutoConnect = true;
    std::string hostname = "esp32-s3-touch";

    // BLE
    std::string bleDeviceName = "ESP32-S3";
    bool bleEnabled = true;
    bool bleAdvertising = true;
    bool bleAutoAdvertising = true;  // Auto restart advertising after disconnect
    uint8_t bleMaxConnections = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;

    // Display & UI
    uint8_t brightness = 80;
    uint8_t screenTimeout = 0;  // 0 = never, in minutes
    bool autoSleep = false;
    bool landscapeLayout = true;

    // LED
    uint8_t ledBrightness = 50;
    bool ledEnabled = true;

    // Audio
    uint8_t audioVolume = 80;
    bool audioEnabled = true;

    // Voice Assistant
    std::string openAiApiKey;
    std::string openAiEndpoint = "https://api.openai.com/v1";
    bool voiceAssistantEnabled = false;
    bool localApiMode = false;  // Toggle between cloud and local Docker APIs
    std::string dockerHostIp = "192.168.1.51";  // IP of Docker host for local APIs
    std::string voiceAssistantSystemPromptTemplate;  // Leave empty to use LittleFS prompt by default
    bool autosendEnabled = true;  // Auto-send transcription in AI chat

    // Whisper STT endpoints
    std::string whisperCloudEndpoint = "https://api.openai.com/v1/audio/transcriptions";
    std::string whisperLocalEndpoint = "http://192.168.1.51:8002/v1/audio/transcriptions";

    // LLM/GPT endpoints
    std::string llmCloudEndpoint = "https://api.openai.com/v1/chat/completions";
    std::string llmLocalEndpoint = "http://192.168.1.51:11434/v1/chat/completions";
    std::string llmModel = "llama3.2:3b";  // Model name for LLM requests

    // TTS (Text-to-Speech) endpoints
    bool ttsEnabled = false;
    std::string ttsCloudEndpoint = "https://api.openai.com/v1/audio/speech";
    std::string ttsLocalEndpoint = "http://192.168.1.51:7778/v1/audio/speech";
    std::string ttsVoice = "if_sara";  // Voice name for TTS (Local: if_sara, af_heart; OpenAI: alloy, echo, fable, onyx, nova, shimmer)
    std::string ttsModel = "hexgrad/Kokoro-82M";  // TTS model name (Local: hexgrad/Kokoro-82M; OpenAI: tts-1)
    float ttsSpeed = 1.0f;  // Speech speed (0.25 to 4.0)
    std::string ttsOutputFormat = "mp3";  // Output format: mp3, opus, aac, flac
    std::string ttsOutputPath = "/memory/audio";  // Where to save TTS audio files

    // Theme
    std::string theme;
    uint32_t primaryColor = 0x0b2035;
    uint32_t accentColor = 0x5df4ff;
    uint32_t cardColor = 0x10182c;
    uint32_t dockColor = 0x1a2332;
    uint32_t dockIconBackgroundColor = 0x16213e;
    uint32_t dockIconSymbolColor = 0xffffff;
    uint8_t dockIconRadius = 24;
    uint8_t borderRadius = 12;

    // Time & NTP
    std::string timezone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Europe/Rome
    std::string ntpServer = "pool.ntp.org";
    std::string ntpServer2 = "time.google.com";
    std::string ntpServer3 = "time.cloudflare.com";
    bool autoTimeSync = true;
    uint32_t timeSyncIntervalHours = 1;

    // Web Data Manager
    std::vector<std::string> webDataAllowedDomains;
    size_t webDataMaxFileSizeKb = 50;
    uint32_t webDataMaxRequestsPerHour = 10;
    uint32_t webDataRequestTimeoutMs = 10000;

    // Storage access whitelist
    std::vector<std::string> storageAllowedSdPaths;
    std::vector<std::string> storageAllowedLittleFsPaths;

    // System
    OperatingMode_t operatingMode = OPERATING_MODE_FULL;
    std::string version;
    uint32_t bootCount = 0;
    uint32_t settingsVersion = 1;  // For migration support
    std::string lastBackupTime;
};

class SettingsManager {
public:
    enum class SettingKey : uint8_t {
        // System
        OperatingMode,
        Version,
        BootCount,

        // WiFi & Network
        WifiSsid,
        WifiPassword,
        WifiAutoConnect,
        Hostname,

        // BLE
        BleDeviceName,
        BleEnabled,
        BleAdvertising,
        BleAutoAdvertising,
        BleMaxConnections,

        // Display & UI
        Brightness,
        ScreenTimeout,
        AutoSleep,
        LayoutOrientation,

        // LED
        LedBrightness,
        LedEnabled,

        // Audio
        AudioVolume,
        AudioEnabled,

        // Voice Assistant
        VoiceAssistantEnabled,
        OpenAIApiKey,
        OpenAIEndpoint,
        LocalApiMode,
        DockerHostIp,
        WhisperCloudEndpoint,
        WhisperLocalEndpoint,
        LlmCloudEndpoint,
        LlmLocalEndpoint,
        LlmModel,
        VoiceAssistantSystemPrompt,
        AutosendEnabled,

        // TTS
        TtsEnabled,
        TtsCloudEndpoint,
        TtsLocalEndpoint,
        TtsVoice,
        TtsModel,
        TtsSpeed,
        TtsOutputFormat,
        TtsOutputPath,

        // Theme
        Theme,
        ThemePrimaryColor,
        ThemeAccentColor,
        ThemeCardColor,
        ThemeDockColor,
        ThemeDockIconBackgroundColor,
        ThemeDockIconSymbolColor,
        ThemeDockIconRadius,
        ThemeBorderRadius,

        // Time & NTP
        Timezone,
        NtpServer,
        NtpServer2,
        NtpServer3,
        AutoTimeSync,
        TimeSyncIntervalHours,

        StorageSdWhitelist,
        StorageLittleFsWhitelist
    };

    using Callback = std::function<void(SettingKey, const SettingsSnapshot&)>;

    static SettingsManager& getInstance();

    bool begin();
    void reset();

    const SettingsSnapshot& getSnapshot() const { return current_; }

    OperatingMode_t getOperatingMode() const { return current_.operatingMode; }
    void setOperatingMode(OperatingMode_t mode);

    const std::string& getWifiSsid() const { return current_.wifiSsid; }
    void setWifiSsid(const std::string& ssid);

    const std::string& getWifiPassword() const { return current_.wifiPassword; }
    void setWifiPassword(const std::string& password);

    uint8_t getBrightness() const { return current_.brightness; }
    void setBrightness(uint8_t value);

    uint8_t getLedBrightness() const { return current_.ledBrightness; }
    void setLedBrightness(uint8_t value);

    const std::string& getTheme() const { return current_.theme; }
    void setTheme(const std::string& theme);

    const std::string& getVersion() const { return current_.version; }
    void setVersion(const std::string& version);

    uint32_t getPrimaryColor() const { return current_.primaryColor; }
    void setPrimaryColor(uint32_t color);

    uint32_t getAccentColor() const { return current_.accentColor; }
    void setAccentColor(uint32_t color);

    uint32_t getCardColor() const { return current_.cardColor; }
    void setCardColor(uint32_t color);

    uint32_t getDockColor() const { return current_.dockColor; }
    void setDockColor(uint32_t color);
    uint32_t getDockIconBackgroundColor() const { return current_.dockIconBackgroundColor; }
    void setDockIconBackgroundColor(uint32_t color);
    uint32_t getDockIconSymbolColor() const { return current_.dockIconSymbolColor; }
    void setDockIconSymbolColor(uint32_t color);
    uint8_t getDockIconRadius() const { return current_.dockIconRadius; }
    void setDockIconRadius(uint8_t radius);

    uint8_t getBorderRadius() const { return current_.borderRadius; }
    void setBorderRadius(uint8_t radius);

    bool isLandscapeLayout() const { return current_.landscapeLayout; }
    void setLandscapeLayout(bool landscape);

    // WiFi Network
    bool getWifiAutoConnect() const { return current_.wifiAutoConnect; }
    void setWifiAutoConnect(bool autoConnect);

    const std::string& getHostname() const { return current_.hostname; }
    void setHostname(const std::string& hostname);

    // BLE
    const std::string& getBleDeviceName() const { return current_.bleDeviceName; }
    void setBleDeviceName(const std::string& name);

    bool getBleEnabled() const { return current_.bleEnabled; }
    void setBleEnabled(bool enabled);

    bool getBleAdvertising() const { return current_.bleAdvertising; }
    void setBleAdvertising(bool advertising);

    bool getBleAutoAdvertising() const { return current_.bleAutoAdvertising; }
    void setBleAutoAdvertising(bool autoAdvertising);

    uint8_t getBleMaxConnections() const { return current_.bleMaxConnections; }
    void setBleMaxConnections(uint8_t maxConnections);

    // Display
    uint8_t getScreenTimeout() const { return current_.screenTimeout; }
    void setScreenTimeout(uint8_t timeout);

    bool getAutoSleep() const { return current_.autoSleep; }
    void setAutoSleep(bool autoSleep);

    // LED
    bool getLedEnabled() const { return current_.ledEnabled; }
    void setLedEnabled(bool enabled);

    // Audio
    uint8_t getAudioVolume() const { return current_.audioVolume; }
    void setAudioVolume(uint8_t volume);

    bool getAudioEnabled() const { return current_.audioEnabled; }
    void setAudioEnabled(bool enabled);

    const std::string& getVoiceAssistantSystemPromptTemplate() const {
        return current_.voiceAssistantSystemPromptTemplate;
    }
    void setVoiceAssistantSystemPromptTemplate(const std::string& prompt);

    // Voice Assistant
    const std::string& getOpenAiApiKey() const { return current_.openAiApiKey; }
    void setOpenAiApiKey(const std::string& key);

    const std::string& getOpenAiEndpoint() const { return current_.openAiEndpoint; }
    void setOpenAiEndpoint(const std::string& endpoint);

    bool getVoiceAssistantEnabled() const { return current_.voiceAssistantEnabled; }
    void setVoiceAssistantEnabled(bool enabled);

    bool getLocalApiMode() const { return current_.localApiMode; }
    void setLocalApiMode(bool enabled);

    const std::string& getDockerHostIp() const { return current_.dockerHostIp; }
    void setDockerHostIp(const std::string& ip);

    const std::string& getWhisperCloudEndpoint() const { return current_.whisperCloudEndpoint; }
    void setWhisperCloudEndpoint(const std::string& endpoint);

    const std::string& getWhisperLocalEndpoint() const { return current_.whisperLocalEndpoint; }
    void setWhisperLocalEndpoint(const std::string& endpoint);

    const std::string& getLlmCloudEndpoint() const { return current_.llmCloudEndpoint; }
    void setLlmCloudEndpoint(const std::string& endpoint);

    const std::string& getLlmLocalEndpoint() const { return current_.llmLocalEndpoint; }
    void setLlmLocalEndpoint(const std::string& endpoint);

    const std::string& getLlmModel() const { return current_.llmModel; }
    void setLlmModel(const std::string& model);

    bool getAutosendEnabled() const { return current_.autosendEnabled; }
    void setAutosendEnabled(bool enabled);

    // TTS
    bool getTtsEnabled() const { return current_.ttsEnabled; }
    void setTtsEnabled(bool enabled);

    const std::string& getTtsCloudEndpoint() const { return current_.ttsCloudEndpoint; }
    void setTtsCloudEndpoint(const std::string& endpoint);

    const std::string& getTtsLocalEndpoint() const { return current_.ttsLocalEndpoint; }
    void setTtsLocalEndpoint(const std::string& endpoint);

    const std::string& getTtsVoice() const { return current_.ttsVoice; }
    void setTtsVoice(const std::string& voice);

    const std::string& getTtsModel() const { return current_.ttsModel; }
    void setTtsModel(const std::string& model);

    float getTtsSpeed() const { return current_.ttsSpeed; }
    void setTtsSpeed(float speed);

    const std::string& getTtsOutputFormat() const { return current_.ttsOutputFormat; }
    void setTtsOutputFormat(const std::string& format);

    const std::string& getTtsOutputPath() const { return current_.ttsOutputPath; }
    void setTtsOutputPath(const std::string& path);

    // Time & NTP
    const std::string& getTimezone() const { return current_.timezone; }
    void setTimezone(const std::string& tz);

    const std::string& getNtpServer() const { return current_.ntpServer; }
    void setNtpServer(const std::string& server);

    const std::string& getNtpServer2() const { return current_.ntpServer2; }
    void setNtpServer2(const std::string& server);

    const std::string& getNtpServer3() const { return current_.ntpServer3; }
    void setNtpServer3(const std::string& server);

    bool getAutoTimeSync() const { return current_.autoTimeSync; }
    void setAutoTimeSync(bool enabled);

    uint32_t getTimeSyncIntervalHours() const { return current_.timeSyncIntervalHours; }
    void setTimeSyncIntervalHours(uint32_t hours);

    // Web Data Manager
    const std::vector<std::string>& getWebDataAllowedDomains() const { return current_.webDataAllowedDomains; }
    void setWebDataAllowedDomains(const std::vector<std::string>& domains);

    size_t getWebDataMaxFileSizeKb() const { return current_.webDataMaxFileSizeKb; }
    void setWebDataMaxFileSizeKb(size_t sizeKb);

    uint32_t getWebDataMaxRequestsPerHour() const { return current_.webDataMaxRequestsPerHour; }
    void setWebDataMaxRequestsPerHour(uint32_t maxRequests);

    uint32_t getWebDataRequestTimeoutMs() const { return current_.webDataRequestTimeoutMs; }
    void setWebDataRequestTimeoutMs(uint32_t timeoutMs);

    const std::vector<std::string>& getStorageAllowedSdPaths() const {
        return current_.storageAllowedSdPaths;
    }
    void setStorageAllowedSdPaths(const std::vector<std::string>& paths);

    const std::vector<std::string>& getStorageAllowedLittleFsPaths() const {
        return current_.storageAllowedLittleFsPaths;
    }
    void setStorageAllowedLittleFsPaths(const std::vector<std::string>& paths);

    // System
    uint32_t getBootCount() const { return current_.bootCount; }
    void incrementBootCount();

    // Backup & Restore
    bool backupToSD();
    bool restoreFromSD();
    bool hasBackup() const;
    const std::string& getLastBackupTime() const { return current_.lastBackupTime; }

    const std::vector<ThemePalette>& getThemePalettes() const { return palettes_; }
    bool addThemePalette(const ThemePalette& palette);

    uint32_t addListener(Callback callback);
    void removeListener(uint32_t id);

private:
    SettingsManager() = default;

    void loadFromStorage();
    void loadDefaults();
    void loadThemePalettes();
    void persistThemePalettes();
    std::vector<ThemePalette> createDefaultPalettes() const;
    void persistSnapshot();
    void notify(SettingKey key);

    struct CallbackEntry {
        uint32_t id;
        Callback fn;
    };

    bool initialized_ = false;
    SettingsSnapshot current_;
    std::vector<ThemePalette> palettes_;
    std::vector<CallbackEntry> callbacks_;
    uint32_t next_callback_id_ = 1;

    // WiFi & Network defaults
    static constexpr bool DEFAULT_WIFI_AUTO_CONNECT = true;
    static constexpr const char* DEFAULT_HOSTNAME = "esp32-s3-touch";

    // BLE defaults
    static constexpr const char* DEFAULT_BLE_DEVICE_NAME = "ESP32-S3";
    static constexpr bool DEFAULT_BLE_ENABLED = true;
    static constexpr bool DEFAULT_BLE_ADVERTISING = true;
    static constexpr bool DEFAULT_BLE_AUTO_ADVERTISING = true;
    static constexpr uint8_t DEFAULT_BLE_MAX_CONNECTIONS = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;

    // Display & UI defaults
    static constexpr uint8_t DEFAULT_BRIGHTNESS = 80;
    static constexpr uint8_t DEFAULT_SCREEN_TIMEOUT = 0;  // Never
    static constexpr bool DEFAULT_AUTO_SLEEP = false;
    static constexpr bool DEFAULT_LANDSCAPE = true;

    // LED defaults
    static constexpr uint8_t DEFAULT_LED_BRIGHTNESS = 50;
    static constexpr bool DEFAULT_LED_ENABLED = true;

    // Audio defaults
    static constexpr uint8_t DEFAULT_AUDIO_VOLUME = 80;
    static constexpr bool DEFAULT_AUDIO_ENABLED = true;

    // Theme defaults
    static constexpr const char* DEFAULT_THEME = "dark";
    static constexpr uint32_t DEFAULT_PRIMARY_COLOR = 0x0b2035;
    static constexpr uint32_t DEFAULT_ACCENT_COLOR = 0x5df4ff;
    static constexpr uint32_t DEFAULT_CARD_COLOR = 0x10182c;
    static constexpr uint32_t DEFAULT_DOCK_COLOR = 0x1a2332;
    static constexpr uint32_t DEFAULT_DOCK_ICON_BG_COLOR = 0x16213e;
    static constexpr uint32_t DEFAULT_DOCK_ICON_SYMBOL_COLOR = 0xffffff;
    static constexpr uint8_t DEFAULT_DOCK_ICON_RADIUS = 24;
    static constexpr uint8_t MAX_DOCK_ICON_RADIUS = 24;
    static constexpr uint8_t DEFAULT_BORDER_RADIUS = 12;

    // System defaults
    static constexpr const char* DEFAULT_VERSION = "0.6.0";
    static constexpr uint32_t SETTINGS_VERSION = 1;
};

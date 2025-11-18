#pragma once

#include <Preferences.h>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

struct SettingsSnapshot {
    std::string wifiSsid;
    std::string wifiPassword;
    uint8_t brightness = 80;
    std::string theme;
    std::string version;
};

class SettingsManager {
public:
    enum class SettingKey : uint8_t {
        WifiSsid,
        WifiPassword,
        Brightness,
        Theme,
        Version
    };

    using Callback = std::function<void(SettingKey, const SettingsSnapshot&)>;

    static SettingsManager& getInstance();

    bool begin();
    void reset();

    const SettingsSnapshot& getSnapshot() const { return current_; }

    const std::string& getWifiSsid() const { return current_.wifiSsid; }
    void setWifiSsid(const std::string& ssid);

    const std::string& getWifiPassword() const { return current_.wifiPassword; }
    void setWifiPassword(const std::string& password);

    uint8_t getBrightness() const { return current_.brightness; }
    void setBrightness(uint8_t value);

    const std::string& getTheme() const { return current_.theme; }
    void setTheme(const std::string& theme);

    const std::string& getVersion() const { return current_.version; }
    void setVersion(const std::string& version);

    uint32_t addListener(Callback callback);
    void removeListener(uint32_t id);

private:
    SettingsManager() = default;

    void loadFromStorage();
    void persistString(const char* key, const std::string& value);
    void notify(SettingKey key);

    struct CallbackEntry {
        uint32_t id;
        Callback fn;
    };

    Preferences prefs_;
    bool initialized_ = false;
    SettingsSnapshot current_;
    std::vector<CallbackEntry> callbacks_;
    uint32_t next_callback_id_ = 1;

    static constexpr const char* NAMESPACE = "os_settings";
    static constexpr const char* KEY_WIFI_SSID = "wifi_ssid";
    static constexpr const char* KEY_WIFI_PASS = "wifi_pass";
    static constexpr const char* KEY_BRIGHTNESS = "brightness";
    static constexpr const char* KEY_THEME = "theme";
    static constexpr const char* KEY_VERSION = "version";
    static constexpr uint8_t DEFAULT_BRIGHTNESS = 80;
    static constexpr const char* DEFAULT_THEME = "dark";
    static constexpr const char* DEFAULT_VERSION = "0.5.0";
};

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
    uint32_t primaryColor = 0x0b2035;
    uint32_t accentColor = 0x5df4ff;
    uint8_t borderRadius = 12;
    bool landscapeLayout = true;
};

class SettingsManager {
public:
    enum class SettingKey : uint8_t {
        WifiSsid,
        WifiPassword,
        Brightness,
        Theme,
        Version,
        ThemePrimaryColor,
        ThemeAccentColor,
        ThemeBorderRadius,
        LayoutOrientation
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

    uint32_t getPrimaryColor() const { return current_.primaryColor; }
    void setPrimaryColor(uint32_t color);

    uint32_t getAccentColor() const { return current_.accentColor; }
    void setAccentColor(uint32_t color);

    uint8_t getBorderRadius() const { return current_.borderRadius; }
    void setBorderRadius(uint8_t radius);

    bool isLandscapeLayout() const { return current_.landscapeLayout; }
    void setLandscapeLayout(bool landscape);

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
    static constexpr const char* KEY_PRIMARY_COLOR = "theme_primary";
    static constexpr const char* KEY_ACCENT_COLOR = "theme_accent";
    static constexpr const char* KEY_BORDER_RADIUS = "theme_radius";
    static constexpr const char* KEY_LAYOUT_ORIENT = "layout_orient";
    static constexpr uint8_t DEFAULT_BRIGHTNESS = 80;
    static constexpr const char* DEFAULT_THEME = "dark";
    static constexpr const char* DEFAULT_VERSION = "0.5.0";
    static constexpr uint32_t DEFAULT_PRIMARY_COLOR = 0x0b2035;
    static constexpr uint32_t DEFAULT_ACCENT_COLOR = 0x5df4ff;
    static constexpr uint8_t DEFAULT_BORDER_RADIUS = 12;
    static constexpr bool DEFAULT_LANDSCAPE = true;
};

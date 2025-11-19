#pragma once

#include "core/theme_palette.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct SettingsSnapshot {
    std::string wifiSsid;
    std::string wifiPassword;
    uint8_t brightness = 80;
    std::string theme;
    std::string version;
    uint32_t primaryColor = 0x0b2035;
    uint32_t accentColor = 0x5df4ff;
    uint32_t cardColor = 0x10182c;
    uint32_t dockColor = 0x1a2332;
    uint32_t dockIconBackgroundColor = 0x16213e;
    uint32_t dockIconSymbolColor = 0xffffff;
    uint8_t dockIconRadius = 24;
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
        ThemeCardColor,
        ThemeDockColor,
        ThemeDockIconBackgroundColor,
        ThemeDockIconSymbolColor,
        ThemeDockIconRadius,
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

    static constexpr uint8_t DEFAULT_BRIGHTNESS = 80;
    static constexpr const char* DEFAULT_THEME = "dark";
    static constexpr const char* DEFAULT_VERSION = "0.5.0";
    static constexpr uint32_t DEFAULT_PRIMARY_COLOR = 0x0b2035;
    static constexpr uint32_t DEFAULT_ACCENT_COLOR = 0x5df4ff;
    static constexpr uint32_t DEFAULT_CARD_COLOR = 0x10182c;
    static constexpr uint32_t DEFAULT_DOCK_COLOR = 0x1a2332;
    static constexpr uint32_t DEFAULT_DOCK_ICON_BG_COLOR = 0x16213e;
    static constexpr uint32_t DEFAULT_DOCK_ICON_SYMBOL_COLOR = 0xffffff;
    static constexpr uint8_t DEFAULT_DOCK_ICON_RADIUS = 24;
    static constexpr uint8_t MAX_DOCK_ICON_RADIUS = 24;
    static constexpr uint8_t DEFAULT_BORDER_RADIUS = 12;
    static constexpr bool DEFAULT_LANDSCAPE = true;
};

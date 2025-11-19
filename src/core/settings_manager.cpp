#include "core/settings_manager.h"
#include "core/storage_manager.h"
#include <Arduino.h>
#include <algorithm>
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include <utility>

namespace {
constexpr size_t MAX_WIFI_FIELD_LENGTH = 63;
struct PaletteSeed {
    const char* name;
    uint32_t primary;
    uint32_t accent;
    uint32_t card;
    uint32_t dock;
};

constexpr PaletteSeed DEFAULT_PALETTE_SEEDS[] = {
    {"Aurora", 0x0b2035, 0x5df4ff, 0x10182c, 0x1a2332},
    {"Sunset", 0x2b1f3a, 0xff7f50, 0x3d2a45, 0x4a3352},
    {"Forest", 0x0f2d1c, 0x7ed957, 0x1a3d28, 0x254d35},
    {"Mono", 0x1a1a1a, 0xffffff, 0x2a2a2a, 0x3a3a3a}
};

std::string sanitizeString(const std::string& value, size_t max_len) {
    if (value.size() <= max_len) {
        return value;
    }
    return value.substr(0, max_len);
}
} // namespace

SettingsManager& SettingsManager::getInstance() {
    static SettingsManager instance;
    return instance;
}

bool SettingsManager::begin() {
    if (initialized_) {
        return true;
    }

    StorageManager& storage = StorageManager::getInstance();
    if (!storage.begin()) {
        Logger::getInstance().error("[Settings] Failed to start storage backend");
        return false;
    }

    loadDefaults();
    initialized_ = true;
    loadFromStorage();
    loadThemePalettes();
    Logger::getInstance().info("[Settings] Manager initialized");
    return true;
}

void SettingsManager::reset() {
    if (!initialized_) {
        return;
    }

    loadDefaults();
    persistSnapshot();

    notify(SettingKey::WifiSsid);
    notify(SettingKey::WifiPassword);
    notify(SettingKey::Brightness);
    notify(SettingKey::Theme);
    notify(SettingKey::Version);
    notify(SettingKey::ThemePrimaryColor);
    notify(SettingKey::ThemeAccentColor);
    notify(SettingKey::ThemeCardColor);
    notify(SettingKey::ThemeDockColor);
    notify(SettingKey::ThemeBorderRadius);
    notify(SettingKey::LayoutOrientation);
}

void SettingsManager::setWifiSsid(const std::string& ssid) {
    if (!initialized_) {
        return;
    }
    const std::string sanitized = sanitizeString(ssid, MAX_WIFI_FIELD_LENGTH);
    if (sanitized == current_.wifiSsid) {
        return;
    }
    current_.wifiSsid = sanitized;
    persistSnapshot();
    notify(SettingKey::WifiSsid);
}

void SettingsManager::setWifiPassword(const std::string& password) {
    if (!initialized_) {
        return;
    }
    const std::string sanitized = sanitizeString(password, MAX_WIFI_FIELD_LENGTH);
    if (sanitized == current_.wifiPassword) {
        return;
    }
    current_.wifiPassword = sanitized;
    persistSnapshot();
    notify(SettingKey::WifiPassword);
}

void SettingsManager::setBrightness(uint8_t value) {
    if (!initialized_) {
        return;
    }
    uint8_t clamped = std::min<uint8_t>(100, std::max<uint8_t>(1, value));
    if (clamped == current_.brightness) {
        return;
    }
    current_.brightness = clamped;
    persistSnapshot();
    notify(SettingKey::Brightness);
}

void SettingsManager::setTheme(const std::string& theme) {
    if (!initialized_) {
        return;
    }
    if (theme == current_.theme) {
        return;
    }
    current_.theme = theme;
    persistSnapshot();
    notify(SettingKey::Theme);
}

void SettingsManager::setVersion(const std::string& version) {
    if (!initialized_) {
        return;
    }
    if (version == current_.version) {
        return;
    }
    current_.version = version;
    persistSnapshot();
    notify(SettingKey::Version);
}

void SettingsManager::setPrimaryColor(uint32_t color) {
    if (!initialized_) {
        return;
    }
    if (color == current_.primaryColor) {
        return;
    }
    current_.primaryColor = color;
    persistSnapshot();
    notify(SettingKey::ThemePrimaryColor);
}

void SettingsManager::setAccentColor(uint32_t color) {
    if (!initialized_) {
        return;
    }
    if (color == current_.accentColor) {
        return;
    }
    current_.accentColor = color;
    persistSnapshot();
    notify(SettingKey::ThemeAccentColor);
}

void SettingsManager::setCardColor(uint32_t color) {
    if (!initialized_) {
        return;
    }
    if (color == current_.cardColor) {
        return;
    }
    current_.cardColor = color;
    persistSnapshot();
    notify(SettingKey::ThemeCardColor);
}

void SettingsManager::setDockColor(uint32_t color) {
    if (!initialized_) {
        return;
    }
    if (color == current_.dockColor) {
        return;
    }
    current_.dockColor = color;
    persistSnapshot();
    notify(SettingKey::ThemeDockColor);
}

void SettingsManager::setBorderRadius(uint8_t radius) {
    if (!initialized_) {
        return;
    }
    uint8_t clamped = std::min<uint8_t>(30, std::max<uint8_t>(0, radius));
    if (clamped == current_.borderRadius) {
        return;
    }
    current_.borderRadius = clamped;
    persistSnapshot();
    notify(SettingKey::ThemeBorderRadius);
}

void SettingsManager::setLandscapeLayout(bool landscape) {
    if (!initialized_) {
        return;
    }
    if (landscape == current_.landscapeLayout) {
        return;
    }
    current_.landscapeLayout = landscape;
    persistSnapshot();
    notify(SettingKey::LayoutOrientation);
}

uint32_t SettingsManager::addListener(Callback callback) {
    const uint32_t id = next_callback_id_++;
    callbacks_.push_back({id, std::move(callback)});
    return id;
}

void SettingsManager::removeListener(uint32_t id) {
    callbacks_.erase(
        std::remove_if(callbacks_.begin(), callbacks_.end(), [id](const CallbackEntry& entry) {
            return entry.id == id;
        }),
        callbacks_.end());
}

void SettingsManager::loadFromStorage() {
    StorageManager& storage = StorageManager::getInstance();
    if (!storage.loadSettings(current_)) {
        persistSnapshot();
    }
    current_.brightness = std::min<uint8_t>(100, std::max<uint8_t>(1, current_.brightness));
    current_.borderRadius = std::min<uint8_t>(30, std::max<uint8_t>(0, current_.borderRadius));

    // Fix corrupted black primary color (from previous conversion bugs)
    if (current_.primaryColor == 0x000000) {
        Logger::getInstance().warn(UI_SYMBOL_WARNING " Fixing corrupted primary color (was black)");
        current_.primaryColor = DEFAULT_PRIMARY_COLOR;
        persistSnapshot();
    }
}

void SettingsManager::loadDefaults() {
    current_.wifiSsid.clear();
    current_.wifiPassword.clear();
    current_.brightness = DEFAULT_BRIGHTNESS;
    current_.theme = DEFAULT_THEME;
    current_.version = DEFAULT_VERSION;
    current_.primaryColor = DEFAULT_PRIMARY_COLOR;
    current_.accentColor = DEFAULT_ACCENT_COLOR;
    current_.cardColor = DEFAULT_CARD_COLOR;
    current_.dockColor = DEFAULT_DOCK_COLOR;
    current_.borderRadius = DEFAULT_BORDER_RADIUS;
    current_.landscapeLayout = DEFAULT_LANDSCAPE;
}

void SettingsManager::loadThemePalettes() {
    StorageManager& storage = StorageManager::getInstance();
    if (!storage.loadThemePalettes(palettes_)) {
        palettes_ = createDefaultPalettes();
        persistThemePalettes();
    }
}

void SettingsManager::persistThemePalettes() {
    if (!initialized_) {
        return;
    }
    StorageManager::getInstance().saveThemePalettes(palettes_);
}

std::vector<ThemePalette> SettingsManager::createDefaultPalettes() const {
    std::vector<ThemePalette> defaults;
    defaults.reserve(sizeof(DEFAULT_PALETTE_SEEDS) / sizeof(DEFAULT_PALETTE_SEEDS[0]));
    for (const auto& seed : DEFAULT_PALETTE_SEEDS) {
        ThemePalette palette;
        palette.name = seed.name;
        palette.primary = seed.primary;
        palette.accent = seed.accent;
        palette.card = seed.card;
        palette.dock = seed.dock;
        defaults.push_back(std::move(palette));
    }
    return defaults;
}

bool SettingsManager::addThemePalette(const ThemePalette& palette) {
    if (!initialized_ || palette.name.empty()) {
        return false;
    }

    auto existing = std::find_if(palettes_.begin(), palettes_.end(), [&](const ThemePalette& entry) {
        return entry.name == palette.name;
    });

    if (existing != palettes_.end()) {
        if (existing->primary == palette.primary &&
            existing->accent == palette.accent &&
            existing->card == palette.card &&
            existing->dock == palette.dock) {
            return false;
        }
        *existing = palette;
    } else {
        palettes_.push_back(palette);
    }

    persistThemePalettes();
    return true;
}

void SettingsManager::persistSnapshot() {
    if (!initialized_) {
        return;
    }
    StorageManager::getInstance().saveSettings(current_);
}

void SettingsManager::notify(SettingKey key) {
    for (const auto& entry : callbacks_) {
        if (entry.fn) {
            entry.fn(key, current_);
        }
    }
}

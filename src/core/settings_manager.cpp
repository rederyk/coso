#include "core/settings_manager.h"
#include "core/storage_manager.h"
#include "drivers/sd_card_driver.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include <LittleFS.h>
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
    uint32_t dockIconBackground;
    uint32_t dockIconSymbol;
    uint8_t dockIconRadius;
};

constexpr PaletteSeed DEFAULT_PALETTE_SEEDS[] = {
    {"Aurora", 0x0b2035, 0x5df4ff, 0x10182c, 0x1a2332, 0x1a2332, 0xffffff, 24},
    {"Sunset", 0x2b1f3a, 0xff7f50, 0x3d2a45, 0x4a3352, 0x4a3352, 0xffffff, 24},
    {"Forest", 0x0f2d1c, 0x7ed957, 0x1a3d28, 0x254d35, 0x254d35, 0xffffff, 24},
    {"Mono", 0x1a1a1a, 0xffffff, 0x2a2a2a, 0x3a3a3a, 0x3a3a3a, 0xffffff, 16}
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
    notify(SettingKey::ThemeDockIconBackgroundColor);
    notify(SettingKey::ThemeDockIconSymbolColor);
    notify(SettingKey::ThemeDockIconRadius);
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

void SettingsManager::setLedBrightness(uint8_t value) {
    if (!initialized_) {
        return;
    }
    uint8_t clamped = std::min<uint8_t>(100, std::max<uint8_t>(0, value));
    if (clamped == current_.ledBrightness) {
        return;
    }
    current_.ledBrightness = clamped;
    persistSnapshot();
    notify(SettingKey::LedBrightness);
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

void SettingsManager::setDockIconBackgroundColor(uint32_t color) {
    if (!initialized_) {
        return;
    }
    if (color == current_.dockIconBackgroundColor) {
        return;
    }
    current_.dockIconBackgroundColor = color;
    persistSnapshot();
    notify(SettingKey::ThemeDockIconBackgroundColor);
}

void SettingsManager::setDockIconSymbolColor(uint32_t color) {
    if (!initialized_) {
        return;
    }
    if (color == current_.dockIconSymbolColor) {
        return;
    }
    current_.dockIconSymbolColor = color;
    persistSnapshot();
    notify(SettingKey::ThemeDockIconSymbolColor);
}

void SettingsManager::setDockIconRadius(uint8_t radius) {
    if (!initialized_) {
        return;
    }
    uint8_t clamped = std::min<uint8_t>(MAX_DOCK_ICON_RADIUS, radius);
    if (clamped == current_.dockIconRadius) {
        return;
    }
    current_.dockIconRadius = clamped;
    persistSnapshot();
    notify(SettingKey::ThemeDockIconRadius);
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
    current_.dockIconRadius = std::min<uint8_t>(MAX_DOCK_ICON_RADIUS, current_.dockIconRadius);

    // Fix corrupted black primary color (from previous conversion bugs)
    if (current_.primaryColor == 0x000000) {
        Logger::getInstance().warn(UI_SYMBOL_WARNING " Fixing corrupted primary color (was black)");
        current_.primaryColor = DEFAULT_PRIMARY_COLOR;
        persistSnapshot();
    }
}

void SettingsManager::loadDefaults() {
    // WiFi & Network
    current_.wifiSsid.clear();
    current_.wifiPassword.clear();
    current_.wifiAutoConnect = DEFAULT_WIFI_AUTO_CONNECT;
    current_.hostname = DEFAULT_HOSTNAME;

    // BLE
    current_.bleDeviceName = DEFAULT_BLE_DEVICE_NAME;
    current_.bleEnabled = DEFAULT_BLE_ENABLED;
    current_.bleAdvertising = DEFAULT_BLE_ADVERTISING;

    // Display & UI
    current_.brightness = DEFAULT_BRIGHTNESS;
    current_.screenTimeout = DEFAULT_SCREEN_TIMEOUT;
    current_.autoSleep = DEFAULT_AUTO_SLEEP;
    current_.landscapeLayout = DEFAULT_LANDSCAPE;

    // LED
    current_.ledBrightness = DEFAULT_LED_BRIGHTNESS;
    current_.ledEnabled = DEFAULT_LED_ENABLED;

    // Audio
    current_.audioVolume = DEFAULT_AUDIO_VOLUME;
    current_.audioEnabled = DEFAULT_AUDIO_ENABLED;

    // Theme
    current_.theme = DEFAULT_THEME;
    current_.primaryColor = DEFAULT_PRIMARY_COLOR;
    current_.accentColor = DEFAULT_ACCENT_COLOR;
    current_.cardColor = DEFAULT_CARD_COLOR;
    current_.dockColor = DEFAULT_DOCK_COLOR;
    current_.dockIconBackgroundColor = DEFAULT_DOCK_ICON_BG_COLOR;
    current_.dockIconSymbolColor = DEFAULT_DOCK_ICON_SYMBOL_COLOR;
    current_.dockIconRadius = DEFAULT_DOCK_ICON_RADIUS;
    current_.borderRadius = DEFAULT_BORDER_RADIUS;

    // System
    current_.version = DEFAULT_VERSION;
    current_.bootCount = 0;
    current_.settingsVersion = SETTINGS_VERSION;
    current_.lastBackupTime.clear();
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
        palette.dockIconBackground = seed.dockIconBackground;
        palette.dockIconSymbol = seed.dockIconSymbol;
        palette.dockIconRadius = seed.dockIconRadius;
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
            existing->dock == palette.dock &&
            existing->dockIconBackground == palette.dockIconBackground &&
            existing->dockIconSymbol == palette.dockIconSymbol &&
            existing->dockIconRadius == palette.dockIconRadius) {
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

// WiFi & Network setters
void SettingsManager::setWifiAutoConnect(bool autoConnect) {
    if (!initialized_ || autoConnect == current_.wifiAutoConnect) {
        return;
    }
    current_.wifiAutoConnect = autoConnect;
    persistSnapshot();
    notify(SettingKey::WifiAutoConnect);
}

void SettingsManager::setHostname(const std::string& hostname) {
    if (!initialized_ || hostname == current_.hostname) {
        return;
    }
    current_.hostname = hostname;
    persistSnapshot();
    notify(SettingKey::Hostname);
}

// BLE setters
void SettingsManager::setBleDeviceName(const std::string& name) {
    if (!initialized_ || name == current_.bleDeviceName) {
        return;
    }
    current_.bleDeviceName = name;
    persistSnapshot();
    notify(SettingKey::BleDeviceName);
}

void SettingsManager::setBleEnabled(bool enabled) {
    if (!initialized_ || enabled == current_.bleEnabled) {
        return;
    }
    current_.bleEnabled = enabled;
    persistSnapshot();
    notify(SettingKey::BleEnabled);
}

void SettingsManager::setBleAdvertising(bool advertising) {
    if (!initialized_ || advertising == current_.bleAdvertising) {
        return;
    }
    current_.bleAdvertising = advertising;
    persistSnapshot();
    notify(SettingKey::BleAdvertising);
}

// Display setters
void SettingsManager::setScreenTimeout(uint8_t timeout) {
    if (!initialized_ || timeout == current_.screenTimeout) {
        return;
    }
    current_.screenTimeout = timeout;
    persistSnapshot();
    notify(SettingKey::ScreenTimeout);
}

void SettingsManager::setAutoSleep(bool autoSleep) {
    if (!initialized_ || autoSleep == current_.autoSleep) {
        return;
    }
    current_.autoSleep = autoSleep;
    persistSnapshot();
    notify(SettingKey::AutoSleep);
}

// LED setters
void SettingsManager::setLedEnabled(bool enabled) {
    if (!initialized_ || enabled == current_.ledEnabled) {
        return;
    }
    current_.ledEnabled = enabled;
    persistSnapshot();
    notify(SettingKey::LedEnabled);
}

// Audio setters
void SettingsManager::setAudioVolume(uint8_t volume) {
    if (!initialized_) {
        return;
    }
    uint8_t clamped = std::min<uint8_t>(100, std::max<uint8_t>(0, volume));
    if (clamped == current_.audioVolume) {
        return;
    }
    current_.audioVolume = clamped;
    persistSnapshot();
    notify(SettingKey::AudioVolume);
}

void SettingsManager::setAudioEnabled(bool enabled) {
    if (!initialized_ || enabled == current_.audioEnabled) {
        return;
    }
    current_.audioEnabled = enabled;
    persistSnapshot();
    notify(SettingKey::AudioEnabled);
}

// System setters
void SettingsManager::incrementBootCount() {
    if (!initialized_) {
        return;
    }
    current_.bootCount++;
    persistSnapshot();
    notify(SettingKey::BootCount);
}

// Backup & Restore
bool SettingsManager::backupToSD() {
    if (!initialized_) {
        Logger::getInstance().error("[Settings] Cannot backup: not initialized");
        return false;
    }

    // Check if SD card driver is available
    SdCardDriver& sd = SdCardDriver::getInstance();
    if (!sd.isMounted()) {
        Logger::getInstance().warn("[Settings] Cannot backup: SD card not mounted");
        return false;
    }

    // Create /config directory if needed
    if (!SD_MMC.exists("/config")) {
        if (!SD_MMC.mkdir("/config")) {
            Logger::getInstance().error("[Settings] Failed to create /config directory");
            return false;
        }
    }

    // Open backup file
    File backup = SD_MMC.open("/config/settings_backup.json", FILE_WRITE);
    if (!backup) {
        Logger::getInstance().error("[Settings] Failed to open backup file for writing");
        return false;
    }

    // Use StorageManager to save (it has the serialization logic)
    backup.close();

    // Use a temporary settings snapshot and save via StorageManager
    // Actually, simpler: just copy the LittleFS settings.json to SD card
    File src = LittleFS.open("/settings.json", FILE_READ);
    if (!src) {
        Logger::getInstance().error("[Settings] Failed to open settings file for reading");
        return false;
    }

    backup = SD_MMC.open("/config/settings_backup.json", FILE_WRITE);
    if (!backup) {
        src.close();
        Logger::getInstance().error("[Settings] Failed to open backup file for writing");
        return false;
    }

    // Copy file contents
    uint8_t buf[512];
    while (src.available()) {
        size_t len = src.read(buf, sizeof(buf));
        backup.write(buf, len);
    }

    src.close();
    backup.close();

    // Update backup timestamp
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lu", millis() / 1000);
    current_.lastBackupTime = timestamp;
    persistSnapshot();

    Logger::getInstance().info("[Settings] Backup created successfully");
    return true;
}

bool SettingsManager::restoreFromSD() {
    if (!initialized_) {
        Logger::getInstance().error("[Settings] Cannot restore: not initialized");
        return false;
    }

    // Check if SD card driver is available
    SdCardDriver& sd = SdCardDriver::getInstance();
    if (!sd.isMounted()) {
        Logger::getInstance().warn("[Settings] Cannot restore: SD card not mounted");
        return false;
    }

    // Check if backup exists
    if (!SD_MMC.exists("/config/settings_backup.json")) {
        Logger::getInstance().warn("[Settings] No backup file found");
        return false;
    }

    // Copy backup from SD to LittleFS
    File backup = SD_MMC.open("/config/settings_backup.json", FILE_READ);
    if (!backup) {
        Logger::getInstance().error("[Settings] Failed to open backup file for reading");
        return false;
    }

    File dest = LittleFS.open("/settings.json", FILE_WRITE);
    if (!dest) {
        backup.close();
        Logger::getInstance().error("[Settings] Failed to open settings file for writing");
        return false;
    }

    // Copy file contents
    uint8_t buf[512];
    while (backup.available()) {
        size_t len = backup.read(buf, sizeof(buf));
        dest.write(buf, len);
    }

    backup.close();
    dest.close();

    // Reload settings from storage
    StorageManager::getInstance().loadSettings(current_);

    Logger::getInstance().info("[Settings] Settings restored from backup");
    return true;
}

bool SettingsManager::hasBackup() const {
    SdCardDriver& sd = SdCardDriver::getInstance();
    return sd.isMounted() && SD_MMC.exists("/config/settings_backup.json");
}

void SettingsManager::notify(SettingKey key) {
    for (const auto& entry : callbacks_) {
        if (entry.fn) {
            entry.fn(key, current_);
        }
    }
}

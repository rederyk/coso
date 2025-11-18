#include "core/settings_manager.h"
#include <Arduino.h>
#include <algorithm>

namespace {
constexpr size_t MAX_WIFI_FIELD_LENGTH = 63;

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

    bool ok = prefs_.begin(NAMESPACE, false);
    if (!ok) {
        Serial.println("[Settings] Failed to open Preferences namespace");
        return false;
    }

    loadFromStorage();
    initialized_ = true;
    Serial.println("[Settings] Manager initialized");
    return true;
}

void SettingsManager::reset() {
    if (!initialized_) {
        return;
    }

    prefs_.clear();
    current_.wifiSsid.clear();
    current_.wifiPassword.clear();
    current_.brightness = DEFAULT_BRIGHTNESS;
    current_.theme = DEFAULT_THEME;
    current_.version = DEFAULT_VERSION;
    current_.primaryColor = DEFAULT_PRIMARY_COLOR;
    current_.accentColor = DEFAULT_ACCENT_COLOR;
    current_.borderRadius = DEFAULT_BORDER_RADIUS;
    current_.landscapeLayout = DEFAULT_LANDSCAPE;

    persistString(KEY_WIFI_SSID, current_.wifiSsid);
    persistString(KEY_WIFI_PASS, current_.wifiPassword);
    prefs_.putUChar(KEY_BRIGHTNESS, current_.brightness);
    persistString(KEY_THEME, current_.theme);
    persistString(KEY_VERSION, current_.version);
    prefs_.putUInt(KEY_PRIMARY_COLOR, current_.primaryColor);
    prefs_.putUInt(KEY_ACCENT_COLOR, current_.accentColor);
    prefs_.putUChar(KEY_BORDER_RADIUS, current_.borderRadius);
    prefs_.putBool(KEY_LAYOUT_ORIENT, current_.landscapeLayout);

    notify(SettingKey::WifiSsid);
    notify(SettingKey::WifiPassword);
    notify(SettingKey::Brightness);
    notify(SettingKey::Theme);
    notify(SettingKey::Version);
    notify(SettingKey::ThemePrimaryColor);
    notify(SettingKey::ThemeAccentColor);
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
    persistString(KEY_WIFI_SSID, current_.wifiSsid);
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
    persistString(KEY_WIFI_PASS, current_.wifiPassword);
    notify(SettingKey::WifiPassword);
}

void SettingsManager::setBrightness(uint8_t value) {
    if (!initialized_) {
        return;
    }
    uint8_t clamped = std::min<uint8_t>(100, std::max<uint8_t>(0, value));
    if (clamped == current_.brightness) {
        return;
    }
    current_.brightness = clamped;
    prefs_.putUChar(KEY_BRIGHTNESS, current_.brightness);
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
    persistString(KEY_THEME, current_.theme);
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
    persistString(KEY_VERSION, current_.version);
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
    prefs_.putUInt(KEY_PRIMARY_COLOR, current_.primaryColor);
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
    prefs_.putUInt(KEY_ACCENT_COLOR, current_.accentColor);
    notify(SettingKey::ThemeAccentColor);
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
    prefs_.putUChar(KEY_BORDER_RADIUS, current_.borderRadius);
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
    prefs_.putBool(KEY_LAYOUT_ORIENT, current_.landscapeLayout);
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
    current_.wifiSsid = prefs_.getString(KEY_WIFI_SSID, "").c_str();
    current_.wifiPassword = prefs_.getString(KEY_WIFI_PASS, "").c_str();
    current_.brightness = prefs_.getUChar(KEY_BRIGHTNESS, DEFAULT_BRIGHTNESS);
    current_.theme = prefs_.getString(KEY_THEME, DEFAULT_THEME).c_str();
    current_.version = prefs_.getString(KEY_VERSION, DEFAULT_VERSION).c_str();
    current_.primaryColor = prefs_.getUInt(KEY_PRIMARY_COLOR, DEFAULT_PRIMARY_COLOR);
    current_.accentColor = prefs_.getUInt(KEY_ACCENT_COLOR, DEFAULT_ACCENT_COLOR);
    current_.borderRadius = prefs_.getUChar(KEY_BORDER_RADIUS, DEFAULT_BORDER_RADIUS);
    current_.landscapeLayout = prefs_.getBool(KEY_LAYOUT_ORIENT, DEFAULT_LANDSCAPE);
    current_.brightness = std::min<uint8_t>(100, std::max<uint8_t>(0, current_.brightness));
    current_.borderRadius = std::min<uint8_t>(30, std::max<uint8_t>(0, current_.borderRadius));
}

void SettingsManager::persistString(const char* key, const std::string& value) {
    if (!initialized_) {
        return;
    }
    prefs_.putString(key, value.c_str());
}

void SettingsManager::notify(SettingKey key) {
    for (const auto& entry : callbacks_) {
        if (entry.fn) {
            entry.fn(key, current_);
        }
    }
}

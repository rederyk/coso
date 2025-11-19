#include "core/storage_manager.h"

#include "core/settings_manager.h"
#include "core/theme_palette.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "utils/logger.h"
#include <utility>

namespace {
void fillJsonFromSnapshot(const SettingsSnapshot& snapshot, JsonDocument& doc) {
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = snapshot.wifiSsid;
    wifi["password"] = snapshot.wifiPassword;

    JsonObject ui = doc["ui"].to<JsonObject>();
    ui["theme"] = snapshot.theme;
    ui["version"] = snapshot.version;
    ui["brightness"] = snapshot.brightness;
    ui["borderRadius"] = snapshot.borderRadius;
    ui["landscape"] = snapshot.landscapeLayout;

    JsonObject palette = doc["palette"].to<JsonObject>();
    palette["primary"] = snapshot.primaryColor;
    palette["accent"] = snapshot.accentColor;
    palette["card"] = snapshot.cardColor;
    palette["dock"] = snapshot.dockColor;
}

void fillSnapshotFromJson(SettingsSnapshot& snapshot, const JsonDocument& doc) {
    snapshot.wifiSsid = doc["wifi"]["ssid"] | snapshot.wifiSsid;
    snapshot.wifiPassword = doc["wifi"]["password"] | snapshot.wifiPassword;
    snapshot.theme = doc["ui"]["theme"] | snapshot.theme;
    snapshot.version = doc["ui"]["version"] | snapshot.version;
    snapshot.brightness = doc["ui"]["brightness"] | snapshot.brightness;
    snapshot.borderRadius = doc["ui"]["borderRadius"] | snapshot.borderRadius;
    snapshot.landscapeLayout = doc["ui"]["landscape"] | snapshot.landscapeLayout;
    snapshot.primaryColor = doc["palette"]["primary"] | snapshot.primaryColor;
    snapshot.accentColor = doc["palette"]["accent"] | snapshot.accentColor;
    snapshot.cardColor = doc["palette"]["card"] | snapshot.cardColor;
    snapshot.dockColor = doc["palette"]["dock"] | snapshot.dockColor;
}

void fillPalettesArray(const std::vector<ThemePalette>& palettes, JsonDocument& doc) {
    JsonArray arr = doc["palettes"].to<JsonArray>();
    for (const auto& palette : palettes) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = palette.name.c_str();
        obj["primary"] = palette.primary;
        obj["accent"] = palette.accent;
        obj["card"] = palette.card;
        obj["dock"] = palette.dock;
    }
}

void fillPalettesFromJson(std::vector<ThemePalette>& palettes, const JsonDocument& doc) {
    JsonArrayConst arr = doc["palettes"].as<JsonArrayConst>();
    if (arr.isNull()) {
        return;
    }
    for (JsonObjectConst entry : arr) {
        ThemePalette palette;
        palette.name = entry["name"] | "";
        palette.primary = entry["primary"] | 0u;
        palette.accent = entry["accent"] | 0u;
        palette.card = entry["card"] | 0u;
        palette.dock = entry["dock"] | 0u;
        if (!palette.name.empty()) {
            palettes.push_back(std::move(palette));
        }
    }
}
} // namespace

StorageManager& StorageManager::getInstance() {
    static StorageManager instance;
    return instance;
}

bool StorageManager::begin() {
    if (initialized_) {
        return true;
    }

    if (!LittleFS.begin(true)) {
        Logger::getInstance().error("[Storage] Failed to mount LittleFS");
        return false;
    }

    if (!ensureDirectory(ICONS_DIR)) {
        Logger::getInstance().error("[Storage] Unable to create icons directory");
        return false;
    }

    if (!ensureDirectory(BACKGROUNDS_DIR)) {
        Logger::getInstance().error("[Storage] Unable to create backgrounds directory");
        return false;
    }

    initialized_ = true;
    Logger::getInstance().info("[Storage] LittleFS mounted");
    return true;
}

bool StorageManager::saveSettings(const SettingsSnapshot& snapshot) {
    if (!initialized_) {
        return false;
    }

    JsonDocument doc;
    fillJsonFromSnapshot(snapshot, doc);

    File file = LittleFS.open(SETTINGS_FILE, FILE_WRITE);
    if (!file) {
        Logger::getInstance().error("[Storage] Failed to open settings file for writing");
        return false;
    }

    const bool ok = serializeJsonPretty(doc, file) > 0;
    file.close();
    if (!ok) {
        Logger::getInstance().error("[Storage] Failed to serialize settings JSON");
    }
    return ok;
}

bool StorageManager::loadSettings(SettingsSnapshot& snapshot) {
    if (!initialized_) {
        return false;
    }

    if (!LittleFS.exists(SETTINGS_FILE)) {
        return false;
    }

    File file = LittleFS.open(SETTINGS_FILE, FILE_READ);
    if (!file) {
        Logger::getInstance().error("[Storage] Failed to open settings file for reading");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        Logger::getInstance().errorf("[Storage] JSON parse error: %s", err.c_str());
        return false;
    }

    fillSnapshotFromJson(snapshot, doc);
    return true;
}

bool StorageManager::saveThemePalettes(const std::vector<ThemePalette>& palettes) {
    if (!initialized_) {
        return false;
    }

    JsonDocument doc;
    fillPalettesArray(palettes, doc);

    File file = LittleFS.open(PALETTES_FILE, FILE_WRITE);
    if (!file) {
        Logger::getInstance().error("[Storage] Failed to open palettes file for writing");
        return false;
    }

    const bool ok = serializeJsonPretty(doc, file) > 0;
    file.close();
    if (!ok) {
        Logger::getInstance().error("[Storage] Failed to serialize palettes JSON");
    }
    return ok;
}

bool StorageManager::loadThemePalettes(std::vector<ThemePalette>& palettes) {
    if (!initialized_) {
        return false;
    }
    if (!LittleFS.exists(PALETTES_FILE)) {
        return false;
    }

    File file = LittleFS.open(PALETTES_FILE, FILE_READ);
    if (!file) {
        Logger::getInstance().error("[Storage] Failed to open palettes file for reading");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        Logger::getInstance().errorf("[Storage] Palette JSON parse error: %s", err.c_str());
        return false;
    }

    palettes.clear();
    fillPalettesFromJson(palettes, doc);
    return !palettes.empty();
}

bool StorageManager::savePngAsset(AssetType type, const std::string& name, const uint8_t* data, size_t length) {
    if (!initialized_ || !data || length == 0) {
        return false;
    }

    const std::string path = getAssetPath(type, name);
    if (path.empty()) {
        return false;
    }

    File file = LittleFS.open(path.c_str(), FILE_WRITE);
    if (!file) {
        Logger::getInstance().errorf("[Storage] Failed to open %s for writing", path.c_str());
        return false;
    }

    size_t written = file.write(data, length);
    file.close();
    if (written != length) {
        Logger::getInstance().errorf("[Storage] Incomplete PNG write (%u/%u)",
                                     static_cast<unsigned>(written),
                                     static_cast<unsigned>(length));
        LittleFS.remove(path.c_str());
        return false;
    }
    return true;
}

bool StorageManager::loadPngAsset(AssetType type, const std::string& name, std::vector<uint8_t>& out) {
    if (!initialized_) {
        return false;
    }

    const std::string path = getAssetPath(type, name);
    if (path.empty() || !LittleFS.exists(path.c_str())) {
        return false;
    }

    File file = LittleFS.open(path.c_str(), FILE_READ);
    if (!file) {
        return false;
    }

    out.clear();
    out.reserve(file.size());
    while (file.available()) {
        out.push_back(file.read());
    }
    file.close();
    return true;
}

bool StorageManager::deletePngAsset(AssetType type, const std::string& name) {
    if (!initialized_) {
        return false;
    }
    const std::string path = getAssetPath(type, name);
    if (path.empty()) {
        return false;
    }
    return LittleFS.remove(path.c_str());
}

bool StorageManager::ensureDirectory(const char* path) {
    if (LittleFS.exists(path)) {
        return true;
    }
    return LittleFS.mkdir(path);
}

std::string StorageManager::getAssetPath(AssetType type, const std::string& name) const {
    if (name.empty()) {
        return {};
    }

    std::string sanitized = name;
    for (auto& c : sanitized) {
        if (c == ' ') {
            c = '_';
        }
    }

    const char* base = (type == AssetType::Background) ? BACKGROUNDS_DIR : ICONS_DIR;
    std::string path = base;
    path += "/";
    path += sanitized;
    if (path.find(".png") == std::string::npos) {
        path += ".png";
    }
    return path;
}

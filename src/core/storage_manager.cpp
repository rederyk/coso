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
    // WiFi & Network
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = snapshot.wifiSsid;
    wifi["password"] = snapshot.wifiPassword;
    wifi["autoConnect"] = snapshot.wifiAutoConnect;
    wifi["hostname"] = snapshot.hostname;

    // BLE
    JsonObject ble = doc["ble"].to<JsonObject>();
    ble["deviceName"] = snapshot.bleDeviceName;
    ble["enabled"] = snapshot.bleEnabled;
    ble["advertising"] = snapshot.bleAdvertising;

    // Display & UI
    JsonObject ui = doc["ui"].to<JsonObject>();
    ui["theme"] = snapshot.theme;
    ui["brightness"] = snapshot.brightness;
    ui["screenTimeout"] = snapshot.screenTimeout;
    ui["autoSleep"] = snapshot.autoSleep;
    ui["borderRadius"] = snapshot.borderRadius;
    ui["landscape"] = snapshot.landscapeLayout;

    // LED
    JsonObject led = doc["led"].to<JsonObject>();
    led["brightness"] = snapshot.ledBrightness;
    led["enabled"] = snapshot.ledEnabled;

    // Audio
    JsonObject audio = doc["audio"].to<JsonObject>();
    audio["volume"] = snapshot.audioVolume;
    audio["enabled"] = snapshot.audioEnabled;

    // Theme palette
    JsonObject palette = doc["palette"].to<JsonObject>();
    palette["primary"] = snapshot.primaryColor;
    palette["accent"] = snapshot.accentColor;
    palette["card"] = snapshot.cardColor;
    palette["dock"] = snapshot.dockColor;
    palette["dockIconBackground"] = snapshot.dockIconBackgroundColor;
    palette["dockIconSymbol"] = snapshot.dockIconSymbolColor;
    palette["dockIconRadius"] = snapshot.dockIconRadius;

    // System
    JsonObject system = doc["system"].to<JsonObject>();
    system["version"] = snapshot.version;
    system["bootCount"] = snapshot.bootCount;
    system["settingsVersion"] = snapshot.settingsVersion;
    system["lastBackupTime"] = snapshot.lastBackupTime;
}

void fillSnapshotFromJson(SettingsSnapshot& snapshot, const JsonDocument& doc) {
    // WiFi & Network
    snapshot.wifiSsid = doc["wifi"]["ssid"] | snapshot.wifiSsid;
    snapshot.wifiPassword = doc["wifi"]["password"] | snapshot.wifiPassword;
    snapshot.wifiAutoConnect = doc["wifi"]["autoConnect"] | snapshot.wifiAutoConnect;
    snapshot.hostname = doc["wifi"]["hostname"] | snapshot.hostname;

    // BLE
    snapshot.bleDeviceName = doc["ble"]["deviceName"] | snapshot.bleDeviceName;
    snapshot.bleEnabled = doc["ble"]["enabled"] | snapshot.bleEnabled;
    snapshot.bleAdvertising = doc["ble"]["advertising"] | snapshot.bleAdvertising;

    // Display & UI
    snapshot.theme = doc["ui"]["theme"] | snapshot.theme;
    snapshot.brightness = doc["ui"]["brightness"] | snapshot.brightness;
    snapshot.screenTimeout = doc["ui"]["screenTimeout"] | snapshot.screenTimeout;
    snapshot.autoSleep = doc["ui"]["autoSleep"] | snapshot.autoSleep;
    snapshot.borderRadius = doc["ui"]["borderRadius"] | snapshot.borderRadius;
    snapshot.landscapeLayout = doc["ui"]["landscape"] | snapshot.landscapeLayout;

    // LED
    snapshot.ledBrightness = doc["led"]["brightness"] | snapshot.ledBrightness;
    snapshot.ledEnabled = doc["led"]["enabled"] | snapshot.ledEnabled;

    // Audio
    snapshot.audioVolume = doc["audio"]["volume"] | snapshot.audioVolume;
    snapshot.audioEnabled = doc["audio"]["enabled"] | snapshot.audioEnabled;

    // Theme palette
    snapshot.primaryColor = doc["palette"]["primary"] | snapshot.primaryColor;
    snapshot.accentColor = doc["palette"]["accent"] | snapshot.accentColor;
    snapshot.cardColor = doc["palette"]["card"] | snapshot.cardColor;
    snapshot.dockColor = doc["palette"]["dock"] | snapshot.dockColor;
    snapshot.dockIconBackgroundColor =
        doc["palette"]["dockIconBackground"] | snapshot.dockIconBackgroundColor;
    snapshot.dockIconSymbolColor =
        doc["palette"]["dockIconSymbol"] | snapshot.dockIconSymbolColor;
    snapshot.dockIconRadius = doc["palette"]["dockIconRadius"] | snapshot.dockIconRadius;

    // System
    snapshot.version = doc["system"]["version"] | snapshot.version;
    snapshot.bootCount = doc["system"]["bootCount"] | snapshot.bootCount;
    snapshot.settingsVersion = doc["system"]["settingsVersion"] | snapshot.settingsVersion;
    snapshot.lastBackupTime = doc["system"]["lastBackupTime"] | snapshot.lastBackupTime;
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
        obj["dockIconBackground"] = palette.dockIconBackground;
        obj["dockIconSymbol"] = palette.dockIconSymbol;
        obj["dockIconRadius"] = palette.dockIconRadius;
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
        palette.dockIconBackground = entry["dockIconBackground"] | palette.dock;
        palette.dockIconSymbol = entry["dockIconSymbol"] | 0xffffffu;
        palette.dockIconRadius = entry["dockIconRadius"] | palette.dockIconRadius;
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

    // Atomic write pattern:
    // 1. Write to temp file first
    // 2. If successful, rename temp to actual file
    // This prevents corruption on power loss during write

    const char* temp_file = "/settings.json.tmp";

    // Serialize to JSON
    JsonDocument doc;
    fillJsonFromSnapshot(snapshot, doc);

    // Write to temporary file
    File file = LittleFS.open(temp_file, FILE_WRITE);
    if (!file) {
        Logger::getInstance().error("[Storage] Failed to open temp settings file for writing");
        return false;
    }

    const bool write_ok = serializeJsonPretty(doc, file) > 0;
    file.close();

    if (!write_ok) {
        Logger::getInstance().error("[Storage] Failed to serialize settings JSON");
        LittleFS.remove(temp_file);
        return false;
    }

    // Atomic rename: temp -> actual
    // Remove old file first (LittleFS doesn't support overwrite with rename)
    if (LittleFS.exists(SETTINGS_FILE)) {
        LittleFS.remove(SETTINGS_FILE);
    }

    if (!LittleFS.rename(temp_file, SETTINGS_FILE)) {
        Logger::getInstance().error("[Storage] Failed to rename temp settings file");
        LittleFS.remove(temp_file);
        return false;
    }

    Logger::getInstance().debug("[Storage] Settings saved atomically");
    return true;
}

bool StorageManager::loadSettings(SettingsSnapshot& snapshot) {
    if (!initialized_) {
        return false;
    }

    const char* temp_file = "/settings.json.tmp";
    bool loaded_from_temp = false;

    // Check if temp file exists (interrupted write)
    if (LittleFS.exists(temp_file) && !LittleFS.exists(SETTINGS_FILE)) {
        Logger::getInstance().warn("[Storage] Recovering from interrupted write - using temp file");
        // Try to use temp file as main file
        if (LittleFS.rename(temp_file, SETTINGS_FILE)) {
            loaded_from_temp = true;
        } else {
            Logger::getInstance().error("[Storage] Failed to recover from temp file");
            LittleFS.remove(temp_file);
            return false;
        }
    } else if (LittleFS.exists(temp_file)) {
        // Temp file exists but so does main file - clean up temp
        Logger::getInstance().debug("[Storage] Cleaning up orphaned temp file");
        LittleFS.remove(temp_file);
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
        Logger::getInstance().errorf("[Storage] JSON parse error: %s - settings corrupted", err.c_str());

        // Try to backup corrupted file for debugging
        if (LittleFS.exists("/settings.json.corrupted")) {
            LittleFS.remove("/settings.json.corrupted");
        }
        LittleFS.rename(SETTINGS_FILE, "/settings.json.corrupted");
        Logger::getInstance().warn("[Storage] Corrupted settings backed up to settings.json.corrupted");

        return false;
    }

    // Validate settings version for migration support
    uint32_t loaded_version = doc["system"]["settingsVersion"] | 0;
    if (loaded_version == 0) {
        Logger::getInstance().warn("[Storage] Old settings format detected - will upgrade");
        // Settings will be upgraded when saved next time
    }

    fillSnapshotFromJson(snapshot, doc);

    if (loaded_from_temp) {
        Logger::getInstance().info("[Storage] Settings recovered from interrupted write");
    }

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

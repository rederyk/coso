#pragma once

#include <LittleFS.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct SettingsSnapshot;

class StorageManager {
public:
    enum class AssetType : uint8_t {
        Icon,
        Background
    };

    static StorageManager& getInstance();

    bool begin();
    bool saveSettings(const SettingsSnapshot& snapshot);
    bool loadSettings(SettingsSnapshot& snapshot);

    bool savePngAsset(AssetType type, const std::string& name, const uint8_t* data, size_t length);
    bool loadPngAsset(AssetType type, const std::string& name, std::vector<uint8_t>& out);
    bool deletePngAsset(AssetType type, const std::string& name);

    bool exists(const char* path) const { return LittleFS.exists(path); }
    bool remove(const char* path) { return LittleFS.remove(path); }

    const char* getSettingsPath() const { return SETTINGS_FILE; }
    const char* getIconsDir() const { return ICONS_DIR; }
    const char* getBackgroundsDir() const { return BACKGROUNDS_DIR; }

private:
    StorageManager() = default;

    bool ensureDirectory(const char* path);
    std::string getAssetPath(AssetType type, const std::string& name) const;

    bool initialized_ = false;

    static constexpr const char* SETTINGS_FILE = "/settings.json";
    static constexpr const char* ICONS_DIR = "/icons";
    static constexpr const char* BACKGROUNDS_DIR = "/backgrounds";
};

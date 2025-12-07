#include "core/storage_access_manager.h"

#include "core/settings_manager.h"
#include "drivers/sd_card_driver.h"
#include "utils/logger.h"

#include <FS.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <freertos/FreeRTOS.h>
#include <set>

namespace {
class SdMutexGuard {
public:
    explicit SdMutexGuard(SdCardDriver& driver, TickType_t timeout = portMAX_DELAY)
        : driver_(driver), locked_(driver_.acquireSdMutex(timeout)) {
    }

    ~SdMutexGuard() {
        if (locked_) {
            driver_.releaseSdMutex();
        }
    }

    bool locked() const { return locked_; }

private:
    SdCardDriver& driver_;
    bool locked_;
};

std::string stripLeadingSlashes(std::string path) {
    while (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    return path;
}
}  // namespace

StorageAccessManager& StorageAccessManager::getInstance() {
    static StorageAccessManager instance;
    return instance;
}

bool StorageAccessManager::begin() {
    if (initialized_) {
        return true;
    }

    if (!LittleFS.exists(kWebDataDir)) {
        LittleFS.mkdir(kWebDataDir);
    }
    if (!LittleFS.exists(kMemoryDir)) {
        LittleFS.mkdir(kMemoryDir);
    }

    reloadPolicy();

    SettingsManager& settings = SettingsManager::getInstance();
    settings_listener_id_ = settings.addListener([this](SettingsManager::SettingKey key, const SettingsSnapshot&) {
        if (key == SettingsManager::SettingKey::StorageSdWhitelist ||
            key == SettingsManager::SettingKey::StorageLittleFsWhitelist) {
            reloadPolicy();
        }
    });

    initialized_ = true;
    return true;
}

bool StorageAccessManager::readWebData(const std::string& filename, std::string& out) const {
    const std::string path = getWebDataPath(filename);
    if (path.empty()) {
        return false;
    }

    if (isAllowedLittleFsPath(path) && readFromLittleFs(path, out)) {
        return true;
    }

    if (isAllowedSdPath(path)) {
        return readFromSd(path, out);
    }

    return false;
}

bool StorageAccessManager::writeWebData(const std::string& filename, const std::string& data) const {
    return writeWebData(filename,
                        reinterpret_cast<const uint8_t*>(data.data()),
                        data.size());
}

bool StorageAccessManager::writeWebData(const std::string& filename, const uint8_t* data, size_t size) const {
    const std::string path = getWebDataPath(filename);
    Logger::getInstance().infof("[StorageAccess] writeWebData: filename=%s, path=%s", filename.c_str(), path.c_str());

    if (path.empty()) {
        Logger::getInstance().warnf("[StorageAccess] writeWebData: path is empty!");
        return false;
    }

    if (!isAllowedLittleFsPath(path)) {
        Logger::getInstance().warnf("[StorageAccess] writeWebData: path '%s' is not allowed!", path.c_str());
        Logger::getInstance().infof("[StorageAccess] Allowed prefixes count: %d", littlefs_allowed_prefixes_.size());
        for (const auto& prefix : littlefs_allowed_prefixes_) {
            Logger::getInstance().infof("[StorageAccess]   - prefix: '%s'", prefix.c_str());
        }
        return false;
    }

    return writeToLittleFs(path, data, size);
}

std::vector<std::string> StorageAccessManager::listWebDataFiles() const {
    std::set<std::string> names;
    if (isAllowedSdPath(kWebDataDir)) {
        for (const auto& entry : listSdDirectory(kWebDataDir)) {
            names.insert(entry);
        }
    }
    if (isAllowedLittleFsPath(kWebDataDir)) {
        for (const auto& entry : listLittleFsDirectory(kWebDataDir)) {
            names.insert(entry);
        }
    }
    return {names.begin(), names.end()};
}

bool StorageAccessManager::deleteWebData(const std::string& filename_or_path) const {
    std::string path = normalizePath(filename_or_path);
    if (path.empty()) {
        return false;
    }
    if (!isAllowedLittleFsPath(path)) {
        return false;
    }
    if (!LittleFS.exists(path.c_str())) {
        return false;
    }
    return LittleFS.remove(path.c_str());
}

std::string StorageAccessManager::getWebDataPath(const std::string& filename) const {
    if (filename.empty()) {
        return {};
    }
    return normalizePath(joinPath(kWebDataDir, filename));
}

bool StorageAccessManager::readMemory(const std::string& filename, std::string& out) const {
    const std::string path = getMemoryPath(filename);
    if (path.empty() || !isAllowedLittleFsPath(path)) {
        return false;
    }
    return readFromLittleFs(path, out);
}

bool StorageAccessManager::writeMemory(const std::string& filename, const std::string& data) const {
    const std::string path = getMemoryPath(filename);
    if (path.empty() || !isAllowedLittleFsPath(path)) {
        return false;
    }
    return writeToLittleFs(path,
                           reinterpret_cast<const uint8_t*>(data.data()),
                           data.size());
}

std::vector<std::string> StorageAccessManager::listMemoryFiles() const {
    if (!isAllowedLittleFsPath(kMemoryDir)) {
        return {};
    }
    return listLittleFsDirectory(kMemoryDir);
}

bool StorageAccessManager::deleteMemory(const std::string& filename) const {
    std::string path = getMemoryPath(filename);
    if (path.empty() || !isAllowedLittleFsPath(path)) {
        return false;
    }
    if (!LittleFS.exists(path.c_str())) {
        return false;
    }
    return LittleFS.remove(path.c_str());
}

std::string StorageAccessManager::getMemoryPath(const std::string& filename) const {
    if (filename.empty()) {
        return {};
    }
    return normalizePath(joinPath(kMemoryDir, filename));
}

void StorageAccessManager::reloadPolicy() {
    const SettingsSnapshot& snapshot = SettingsManager::getInstance().getSnapshot();

    auto normalize_list = [this](const std::vector<std::string>& source, std::vector<std::string>& target) {
        target.clear();
        target.reserve(source.size());
        for (const auto& entry : source) {
            if (entry.empty()) {
                continue;
            }
            const std::string normalized = normalizePath(entry);
            if (!normalized.empty()) {
                target.push_back(normalized);
            }
        }
    };

    normalize_list(snapshot.storageAllowedSdPaths, sd_allowed_prefixes_);
    normalize_list(snapshot.storageAllowedLittleFsPaths, littlefs_allowed_prefixes_);
}

std::string StorageAccessManager::normalizePath(std::string path) const {
    if (path.empty()) {
        return "/";
    }
    if (path.front() != '/') {
        path.insert(path.begin(), '/');
    }

    // Accept paths that include the LittleFS mount prefix as reported by ESP-IDF
    constexpr const char kLittleFsPrefix[] = "/littlefs";
    const size_t prefix_len = sizeof(kLittleFsPrefix) - 1;
    if (path.rfind(kLittleFsPrefix, 0) == 0) {
        if (path.size() == prefix_len) {
            path = "/";
        } else {
            path.erase(0, prefix_len);
            if (path.empty()) {
                path = "/";
            }
        }
    }

    std::vector<std::string> segments;
    size_t pos = 0;
    while (pos < path.size()) {
        size_t next = path.find('/', pos);
        if (next == std::string::npos) {
            next = path.size();
        }
        std::string segment = path.substr(pos, next - pos);
        if (!segment.empty() && segment != ".") {
            if (segment == "..") {
                if (!segments.empty()) {
                    segments.pop_back();
                }
            } else {
                segments.push_back(segment);
            }
        }
        pos = next + 1;
    }

    std::string result = "/";
    for (size_t i = 0; i < segments.size(); ++i) {
        result += segments[i];
        if (i + 1 < segments.size()) {
            result += "/";
        }
    }
    return result;
}

std::string StorageAccessManager::joinPath(const std::string& base, const std::string& child) const {
    if (base.empty()) {
        return normalizePath(child);
    }
    std::string trimmed = stripLeadingSlashes(child);
    std::string combined = base;
    if (!combined.empty() && combined.back() == '/') {
        combined.pop_back();
    }
    if (!trimmed.empty()) {
        combined += "/";
        combined += trimmed;
    }
    return normalizePath(combined);
}

bool StorageAccessManager::isAllowedSdPath(const std::string& path) const {
    if (sd_allowed_prefixes_.empty()) {
        return false;
    }
    const std::string normalized = normalizePath(path);
    for (const auto& prefix : sd_allowed_prefixes_) {
        if (prefix.empty()) {
            continue;
        }
        if (normalized == prefix) {
            return true;
        }
        std::string prefix_with_slash = prefix;
        if (!prefix_with_slash.empty() && prefix_with_slash.back() != '/') {
            prefix_with_slash += "/";
        }
        if (normalized.rfind(prefix_with_slash, 0) == 0) {
            return true;
        }
    }
    return false;
}

bool StorageAccessManager::isAllowedLittleFsPath(const std::string& path) const {
    if (littlefs_allowed_prefixes_.empty()) {
        return false;
    }
    const std::string normalized = normalizePath(path);
    for (const auto& prefix : littlefs_allowed_prefixes_) {
        if (prefix.empty()) {
            continue;
        }
        if (normalized == prefix) {
            return true;
        }
        std::string prefix_with_slash = prefix;
        if (!prefix_with_slash.empty() && prefix_with_slash.back() != '/') {
            prefix_with_slash += "/";
        }
        if (normalized.rfind(prefix_with_slash, 0) == 0) {
            return true;
        }
    }
    return false;
}

bool StorageAccessManager::readFromSd(const std::string& path, std::string& out) const {
    SdCardDriver& driver = SdCardDriver::getInstance();
    if (!driver.begin()) {
        Logger::getInstance().warn("[StorageAccess] Failed to mount SD card");
        return false;
    }
    SdMutexGuard guard(driver);
    if (!guard.locked()) {
        Logger::getInstance().warn("[StorageAccess] SD mutex locked by another task");
        return false;
    }

    File file = SD_MMC.open(path.c_str(), FILE_READ);
    if (!file) {
        return false;
    }

    const size_t size = file.size();
    if (size == 0) {
        file.close();
        return false;
    }

    out.resize(size);
    size_t read = file.read(reinterpret_cast<uint8_t*>(const_cast<char*>(out.data())), size);
    file.close();
    if (read != size) {
        Logger::getInstance().warnf("[StorageAccess] Partial SD read for %s", path.c_str());
        return false;
    }
    return true;
}

bool StorageAccessManager::readFromLittleFs(const std::string& path, std::string& out) const {
    if (!LittleFS.exists(path.c_str())) {
        return false;
    }

    File file = LittleFS.open(path.c_str(), FILE_READ);
    if (!file) {
        return false;
    }

    const size_t size = file.size();
    if (size == 0) {
        file.close();
        return false;
    }

    out.resize(size);
    size_t read = file.read(reinterpret_cast<uint8_t*>(const_cast<char*>(out.data())), size);
    file.close();
    if (read != size) {
        Logger::getInstance().warnf("[StorageAccess] Partial LittleFS read for %s", path.c_str());
        return false;
    }
    return true;
}

bool StorageAccessManager::writeToLittleFs(const std::string& path, const uint8_t* data, size_t size) const {
    if (path.empty()) {
        return false;
    }

    size_t slash_pos = path.find_last_of('/');
    if (slash_pos != std::string::npos && slash_pos > 0) {
        std::string parent = path.substr(0, slash_pos);
        if (!LittleFS.exists(parent.c_str())) {
            LittleFS.mkdir(parent.c_str());
        }
    }

    File file = LittleFS.open(path.c_str(), FILE_WRITE);
    if (!file) {
        Logger::getInstance().warnf("[StorageAccess] Failed to open %s for writing", path.c_str());
        return false;
    }

    size_t written = file.write(data, size);
    file.close();

    if (written != size) {
        Logger::getInstance().warnf("[StorageAccess] Incomplete write to %s", path.c_str());
        return false;
    }
    return true;
}

std::vector<std::string> StorageAccessManager::listSdDirectory(const std::string& path) const {
    std::vector<std::string> entries;
    SdCardDriver& driver = SdCardDriver::getInstance();
    if (!driver.begin()) {
        return entries;
    }

    SdMutexGuard guard(driver);
    if (!guard.locked()) {
        return entries;
    }

    File dir = SD_MMC.open(path.c_str());
    if (!dir || !dir.isDirectory()) {
        return entries;
    }

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        if (!entry.isDirectory()) {
            std::string name(entry.name());
            if (name.rfind(path + "/", 0) == 0 && name.size() > path.size() + 1) {
                name = name.substr(path.size() + 1);
            } else if (name == path) {
                name.clear();
            }
            if (!name.empty()) {
                entries.push_back(name);
            }
        }
        entry.close();
    }
    dir.close();

    return entries;
}

std::vector<std::string> StorageAccessManager::listLittleFsDirectory(const std::string& path) const {
    std::vector<std::string> entries;
    File dir = LittleFS.open(path.c_str());
    if (!dir || !dir.isDirectory()) {
        return entries;
    }

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        if (!entry.isDirectory()) {
            std::string name(entry.name());
            if (name.rfind(path + "/", 0) == 0 && name.size() > path.size() + 1) {
                name = name.substr(path.size() + 1);
            } else if (name == path) {
                name.clear();
            }
            if (!name.empty()) {
                entries.push_back(name);
            }
        }
        entry.close();
    }
    dir.close();

    return entries;
}

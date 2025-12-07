#include "core/storage_access_manager.h"

#include "core/settings_manager.h"
#include "drivers/sd_card_driver.h"
#include "utils/logger.h"

#include <FS.h>
#include <initializer_list>
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

// Convert a normalized path ("/webdata/foo") to the path expected by SD_MMC
// (relative to the mount point). It also strips an eventual "/sd" prefix that
// we might receive from logging or user input.
std::string toSdFsPath(const std::string& normalized_path) {
    if (normalized_path.empty()) {
        return "/";
    }
    if (normalized_path == "/sd") {
        return "/";
    }
    if (normalized_path.rfind("/sd/", 0) == 0) {
        std::string without_mount = normalized_path.substr(3);
        return without_mount.empty() ? "/" : without_mount;
    }
    return normalized_path;
}

// Build a display-friendly SD path (always with "/sd" prefix) for logging.
std::string toSdLogPath(const std::string& normalized_path) {
    if (normalized_path.rfind("/sd", 0) == 0) {
        return normalized_path;
    }
    if (normalized_path == "/") {
        return "/sd";
    }
    return "/sd" + normalized_path;
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
        if (!LittleFS.mkdir(kWebDataDir)) {
            Logger::getInstance().warnf("[StorageAccess] Failed to create LittleFS directory: %s", kWebDataDir);
        }
    }
    if (!LittleFS.exists(kMemoryDir)) {
        if (!LittleFS.mkdir(kMemoryDir)) {
            Logger::getInstance().warnf("[StorageAccess] Failed to create LittleFS directory: %s", kMemoryDir);
        }
    }

    // Ensure webdata directory exists on SD if mounted
    SdCardDriver& driver = SdCardDriver::getInstance();
    if (driver.begin()) {
        auto ensureSdDirectory = [&](const char* dir) {
            const std::string normalized = normalizePath(dir);
            const std::string sd_path = toSdFsPath(normalized);
            const std::string log_path = toSdLogPath(normalized);

            SdMutexGuard guard(driver, pdMS_TO_TICKS(500));
            if (!guard.locked()) {
                Logger::getInstance().warnf("[StorageAccess] SD mutex locked, skipped directory check for %s", log_path.c_str());
                return;
            }

            if (!SD_MMC.exists(sd_path.c_str())) {
                if (SD_MMC.mkdir(sd_path.c_str())) {
                    Logger::getInstance().infof("[StorageAccess] Created SD directory: %s", log_path.c_str());
                } else {
                    Logger::getInstance().warnf("[StorageAccess] Failed to create SD directory: %s", log_path.c_str());
                }
            }
        };

        ensureSdDirectory(kWebDataDir);
        ensureSdDirectory(kMemoryDir);
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

    // Prioritize SD for read
    if (isAllowedSdPath(path) && readFromSd(path, out)) {
        Logger::getInstance().infof("[StorageAccess] readWebData: Successfully read from SD: %s", path.c_str());
        return true;
    } else if (isAllowedSdPath(path)) {
        Logger::getInstance().warnf("[StorageAccess] readWebData: Failed to read from SD: %s", path.c_str());
    }

    // Fallback to LittleFS
    if (isAllowedLittleFsPath(path) && readFromLittleFs(path, out)) {
        Logger::getInstance().infof("[StorageAccess] readWebData: Successfully read from LittleFS (fallback): %s", path.c_str());
        return true;
    }

    Logger::getInstance().warnf("[StorageAccess] readWebData: Unable to read %s from SD or LittleFS", path.c_str());
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

    // Prioritize SD for write
    if (isAllowedSdPath(path)) {
        if (writeToSd(path, data, size)) {
            Logger::getInstance().infof("[StorageAccess] writeWebData: Successfully written to SD: %s", path.c_str());
            return true;
        } else {
            Logger::getInstance().warnf("[StorageAccess] writeWebData: Failed to write to SD: %s", path.c_str());
        }
    } else {
        Logger::getInstance().warnf("[StorageAccess] writeWebData: SD path '%s' not allowed!", path.c_str());
    }

    // Fallback to LittleFS
    if (isAllowedLittleFsPath(path)) {
        return writeToLittleFs(path, data, size);
    } else {
        Logger::getInstance().warnf("[StorageAccess] writeWebData: LittleFS path '%s' not allowed!", path.c_str());
        Logger::getInstance().infof("[StorageAccess] Allowed prefixes count: %d (SD), %d (LittleFS)", sd_allowed_prefixes_.size(), littlefs_allowed_prefixes_.size());
        for (const auto& prefix : sd_allowed_prefixes_) {
            Logger::getInstance().infof("[StorageAccess] SD prefix: '%s'", prefix.c_str());
        }
        for (const auto& prefix : littlefs_allowed_prefixes_) {
            Logger::getInstance().infof("[StorageAccess] LittleFS prefix: '%s'", prefix.c_str());
        }
        return false;
    }
}

std::vector<std::string> StorageAccessManager::listWebDataFiles() const {
    std::set<std::string> names;
    // Prioritize SD for listing
    if (isAllowedSdPath(kWebDataDir)) {
        for (const auto& entry : listSdDirectory(kWebDataDir)) {
            names.insert(entry);
        }
        Logger::getInstance().infof("[StorageAccess] listWebDataFiles: Listed %zu files from SD: %s", listSdDirectory(kWebDataDir).size(), kWebDataDir);
    }
    // Merge with LittleFS as fallback only if not already present (avoid duplicates)
    if (isAllowedLittleFsPath(kWebDataDir)) {
        for (const auto& entry : listLittleFsDirectory(kWebDataDir)) {
            if (names.find(entry) == names.end()) {
                names.insert(entry);
            }
        }
        Logger::getInstance().infof("[StorageAccess] listWebDataFiles: Merged %zu additional files from LittleFS fallback: %s", listLittleFsDirectory(kWebDataDir).size(), kWebDataDir);
    }
    return {names.begin(), names.end()};
}

bool StorageAccessManager::deleteWebData(const std::string& filename_or_path) const {
    std::string path = normalizePath(filename_or_path);
    if (path.empty()) {
        return false;
    }

    // Prioritize SD for delete

    if (isAllowedSdPath(path)) {
        SdCardDriver& driver = SdCardDriver::getInstance();
        if (driver.begin()) {
            SdMutexGuard guard(driver);

            if (guard.locked()) {
                const std::string sd_path = toSdFsPath(path);
                const std::string log_path = toSdLogPath(path);
                if (SD_MMC.exists(sd_path.c_str())) {
                    if (SD_MMC.remove(sd_path.c_str())) {
                        Logger::getInstance().infof("[StorageAccess] deleteWebData: Successfully deleted from SD: %s", log_path.c_str());
                        return true;
                    } else {
                        Logger::getInstance().warnf("[StorageAccess] deleteWebData: Failed to delete from SD: %s", log_path.c_str());
                    }
                }
            }
        }
    }

    // Fallback to LittleFS
    if (isAllowedLittleFsPath(path)) {
        if (LittleFS.exists(path.c_str())) {
            if (LittleFS.remove(path.c_str())) {
                Logger::getInstance().infof("[StorageAccess] deleteWebData: Successfully deleted from LittleFS (fallback): %s", path.c_str());
                return true;
            } else {
                Logger::getInstance().warnf("[StorageAccess] deleteWebData: Failed to delete from LittleFS: %s", path.c_str());
            }
        }
    }

    return false;
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

    auto ensure_defaults = [this](std::vector<std::string>& target,
                                  std::initializer_list<const char*> defaults,
                                  const char* label) {
        if (!target.empty()) {
            return;
        }

        Logger::getInstance().warnf("[StorageAccess] %s whitelist empty, applying defaults", label);
        for (const char* entry : defaults) {
            const std::string normalized = normalizePath(entry);
            if (!normalized.empty()) {
                target.push_back(normalized);
            }
        }
    };

    normalize_list(snapshot.storageAllowedSdPaths, sd_allowed_prefixes_);
    normalize_list(snapshot.storageAllowedLittleFsPaths, littlefs_allowed_prefixes_);

    // If settings.json is missing or corrupt we still want /webdata to stay writable.
    ensure_defaults(sd_allowed_prefixes_, {kWebDataDir, kMemoryDir, "/userDir"}, "SD");
    ensure_defaults(littlefs_allowed_prefixes_, {kWebDataDir, kMemoryDir}, "LittleFS");
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

    const std::string sd_path = toSdFsPath(path);
    const std::string sd_log_path = toSdLogPath(path);

    File file = SD_MMC.open(sd_path.c_str(), FILE_READ);
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
        Logger::getInstance().warnf("[StorageAccess] Partial SD read for %s", sd_log_path.c_str());
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

    // Recursive directory creation lambda
    auto ensureLittleFsDirectoryRecursive = [&]() -> bool {
        if (path.empty()) return true;
        size_t last_slash = path.find_last_of('/');
        if (last_slash == std::string::npos || last_slash == 0) return true; // root
        std::string dir_path = path.substr(0, last_slash);

        std::string current = "/";
        size_t idx = 1;
        while (idx < dir_path.length()) {
            size_t next_slash = dir_path.find('/', idx);
            if (next_slash == std::string::npos) next_slash = dir_path.length();
            std::string next_dir = dir_path.substr(0, next_slash);
            if (next_dir.length() > current.length() && !LittleFS.exists(next_dir.c_str())) {
                if (!LittleFS.mkdir(next_dir.c_str())) {
                    Logger::getInstance().warnf("[StorageAccess] Failed to create LittleFS directory: %s", next_dir.c_str());
                    return false;
                }
                current = next_dir;
            }
            idx = next_slash + 1;
        }
        return true;
    };

    // Ensure directory exists recursively on LittleFS
    if (!ensureLittleFsDirectoryRecursive()) {
        return false;
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
    Logger::getInstance().infof("[StorageAccess] Successfully written to LittleFS: %s (%zu bytes)", path.c_str(), size);
    return true;
}



bool StorageAccessManager::writeToSd(const std::string& path, const uint8_t* data, size_t size) const {
    SdCardDriver& driver = SdCardDriver::getInstance();
    if (!driver.begin()) {
        Logger::getInstance().warn("[StorageAccess] Failed to mount SD for write");
        return false;
    }

    SdMutexGuard guard(driver);
    if (!guard.locked()) {
        Logger::getInstance().warn("[StorageAccess] SD mutex locked during write");
        return false;
    }

    std::string sd_path = toSdFsPath(path);
    const std::string sd_log_path = toSdLogPath(path);

    // Recursive directory creation lambda
    auto ensureSdDirectoryRecursive = [&]() -> bool {
        if (sd_path.empty()) return true;
        size_t last_slash = sd_path.find_last_of('/');
        if (last_slash == std::string::npos || last_slash == 0) return true; // root
        std::string dir_path = sd_path.substr(0, last_slash);

        std::string current = "/";
        size_t idx = 1; // after root slash
        while (idx < dir_path.length()) {
            size_t next_slash = dir_path.find('/', idx);
            if (next_slash == std::string::npos) next_slash = dir_path.length();
            std::string next_dir = dir_path.substr(0, next_slash);
            if (next_dir.length() > current.length() && !SD_MMC.exists(next_dir.c_str())) {
                if (!SD_MMC.mkdir(next_dir.c_str())) {
                    Logger::getInstance().warnf("[StorageAccess] Failed to create SD directory: %s", next_dir.c_str());
                    return false;
                }
                current = next_dir;
            }
            idx = next_slash + 1;
        }
        return true;
    };

    // Ensure directory exists recursively on SD
    if (!ensureSdDirectoryRecursive()) {
        return false;
    }

    File file = SD_MMC.open(sd_path.c_str(), FILE_WRITE);
    if (!file) {
        Logger::getInstance().warnf("[StorageAccess] Failed to open %s for SD write", sd_log_path.c_str());
        return false;
    }

    size_t written = file.write(data, size);
    file.close();

    if (written != size) {
        Logger::getInstance().warnf("[StorageAccess] Incomplete SD write to %s (%zu/%zu bytes)", sd_log_path.c_str(), written, size);
        return false;
    }
    Logger::getInstance().infof("[StorageAccess] Successfully written to SD: %s (%zu bytes)", sd_log_path.c_str(), size);
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

    std::string sd_path = toSdFsPath(path);
    File dir = SD_MMC.open(sd_path.c_str());
    if (!dir || !dir.isDirectory()) {
        return entries;
    }

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        if (!entry.isDirectory()) {
            std::string full_name = toSdFsPath(entry.name());
            std::string name = full_name;
            if (sd_path == "/") {
                if (!name.empty() && name.front() == '/') {
                    name.erase(name.begin());
                }
            } else if (name.rfind(sd_path + "/", 0) == 0 && name.size() > sd_path.size() + 1) {
                name = name.substr(sd_path.size() + 1); // Extract relative name
            } else if (name == sd_path) {
                name.clear();
            }
            if (name.empty()) {
                entry.close();
                continue;
            }
            entries.push_back(name);
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

#include "core/memory_manager.h"
#include "core/settings_manager.h"
#include "core/storage_access_manager.h"
#include "core/storage_manager.h"
#include <memory.h>

MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}

bool MemoryManager::init() {
    if (initialized_) return true;

    // Load default permissions
    directory_permissions_["/memory"] = {"", true, true, false};
    directory_permissions_["/userDir"] = {"", true, true, true};

    // TODO: Load from settings when settings are updated to include memory.directories
    // For now, use defaults as specified in plan

    initialized_ = true;
    return true;
}

std::string MemoryManager::readData(const std::string& filename) {
    if (!initialized_) return "";

    std::string directory = "/memory";  // Assume memory files are in /memory for now
    if (!isPathAllowed(directory, "read")) {
        ESP_LOGE("MemoryManager", "Read access denied for directory: %s", directory.c_str());
        return "";
    }

    std::string result;
    if (StorageAccessManager::getInstance().readMemory(filename, result)) {
        return result;
    }
    return "";
}

bool MemoryManager::writeData(const std::string& filename, const std::string& data) {
    if (!initialized_) return false;

    std::string directory = "/memory";  // Assume memory files are in /memory for now
    if (!isPathAllowed(directory, "write")) {
        ESP_LOGE("MemoryManager", "Write access denied for directory: %s", directory.c_str());
        return false;
    }

    return StorageAccessManager::getInstance().writeMemory(filename, data);
}

bool MemoryManager::deleteData(const std::string& filename) {
    if (!initialized_) return false;

    std::string directory = "/memory";  // Assume memory files are in /memory for now
    if (!isPathAllowed(directory, "delete")) {
        ESP_LOGE("MemoryManager", "Delete access denied for directory: %s", directory.c_str());
        return false;
    }

    return StorageAccessManager::getInstance().deleteMemory(filename);
}

std::vector<std::string> MemoryManager::listFiles(const std::string& directory) {
    if (!initialized_) return {};

    std::string dir = directory.empty() ? "/memory" : directory;
    if (!isPathAllowed(dir, "read")) {
        ESP_LOGE("MemoryManager", "List access denied for directory: %s", dir.c_str());
        return {};
    }

    return StorageAccessManager::getInstance().listMemoryFiles();
}

bool MemoryManager::setDirectoryPermissions(const std::string& path, bool can_read, bool can_write, bool can_delete) {
    directory_permissions_[path] = {path, can_read, can_write, can_delete};
    // TODO: Persist to settings
    return true;
}

DirectoryPermissions MemoryManager::getDirectoryPermissions(const std::string& path) {
    if (directory_permissions_.find(path) != directory_permissions_.end()) {
        return directory_permissions_[path];
    }
    // Default deny
    return {path, false, false, false};
}

std::map<std::string, DirectoryPermissions> MemoryManager::getAllDirectoryPermissions() {
    return directory_permissions_;
}

bool MemoryManager::isPathAllowed(const std::string& path, const std::string& operation) {
    std::string dir = getDirectoryFromPath(path);
    DirectoryPermissions perms = getDirectoryPermissions(dir);

    if (operation == "read") return perms.can_read;
    if (operation == "write") return perms.can_write;
    if (operation == "delete") return perms.can_delete;

    return false;
}

std::string MemoryManager::normalizePath(const std::string& path) {
    if (path.empty() || path[0] != '/') return "/" + path;
    return path;
}

std::string MemoryManager::getDirectoryFromPath(const std::string& path) {
    std::string normalized = normalizePath(path);
    if (normalized == "/" || normalized.empty()) {
        return "/";
    }

    if (normalized.back() == '/') {
        normalized.pop_back();
    }

    size_t lastSlash = normalized.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return normalized;
    }

    // When the path has only one leading slash (e.g. "/memory"), return the full path
    if (lastSlash == 0) {
        return normalized;
    }

    return normalized.substr(0, lastSlash);
}

std::string MemoryManager::getMemoryPath(const std::string& filename) {
    return StorageAccessManager::getInstance().getMemoryPath(filename);
}

MemoryManager::MemoryManager() : initialized_(false) {}

MemoryManager::~MemoryManager() {}

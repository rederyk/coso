#pragma once

#include <string>
#include <vector>

class StorageAccessManager {
public:
    static StorageAccessManager& getInstance();

    bool begin();

    bool readWebData(const std::string& filename, std::string& out) const;
    bool writeWebData(const std::string& filename, const std::string& data) const;
    bool writeWebData(const std::string& filename, const uint8_t* data, size_t size) const;
    std::vector<std::string> listWebDataFiles() const;
    bool deleteWebData(const std::string& filename_or_path) const;
    std::string getWebDataPath(const std::string& filename) const;

    bool readMemory(const std::string& filename, std::string& out) const;
    bool writeMemory(const std::string& filename, const std::string& data) const;
    bool deleteMemory(const std::string& filename) const;
    std::vector<std::string> listMemoryFiles() const;
    std::string getMemoryPath(const std::string& filename) const;

private:
    StorageAccessManager() = default;
    void reloadPolicy();
    std::string normalizePath(std::string path) const;
    std::string joinPath(const std::string& base, const std::string& child) const;
    bool isAllowedSdPath(const std::string& path) const;
    bool isAllowedLittleFsPath(const std::string& path) const;
    bool readFromSd(const std::string& path, std::string& out) const;
    bool readFromLittleFs(const std::string& path, std::string& out) const;
    bool writeToLittleFs(const std::string& path, const uint8_t* data, size_t size) const;
    bool writeToSd(const std::string& path, const uint8_t* data, size_t size) const;
    std::vector<std::string> listSdDirectory(const std::string& path) const;
    std::vector<std::string> listLittleFsDirectory(const std::string& path) const;

    bool initialized_ = false;
    uint32_t settings_listener_id_ = 0;
    std::vector<std::string> sd_allowed_prefixes_;
    std::vector<std::string> littlefs_allowed_prefixes_;

    static constexpr const char* kWebDataDir = "/webdata";
    static constexpr const char* kMemoryDir = "/memory";
};

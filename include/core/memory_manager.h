#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <string>
#include <vector>
#include <map>

struct DirectoryPermissions {
    std::string path;
    bool can_read;
    bool can_write;
    bool can_delete;
};

class MemoryManager {
public:
    static MemoryManager& getInstance();

    // Initialization
    bool init();

    // CRUD operations with permission checking
    std::string readData(const std::string& filename);
    bool writeData(const std::string& filename, const std::string& data);
    bool deleteData(const std::string& filename);
    std::vector<std::string> listFiles(const std::string& directory = "");

    // Configuration and permissions
    bool setDirectoryPermissions(const std::string& path, bool can_read, bool can_write, bool can_delete);
    DirectoryPermissions getDirectoryPermissions(const std::string& path);
    std::map<std::string, DirectoryPermissions> getAllDirectoryPermissions();

    // Path validation
    bool isPathAllowed(const std::string& path, const std::string& operation);

private:
    MemoryManager(); // Singleton
    ~MemoryManager();
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    std::map<std::string, DirectoryPermissions> directory_permissions_;
    bool initialized_;

    // Helper methods
    std::string normalizePath(const std::string& path);
    std::string getDirectoryFromPath(const std::string& path);
    std::string getMemoryPath(const std::string& filename);
};

#endif // MEMORY_MANAGER_H

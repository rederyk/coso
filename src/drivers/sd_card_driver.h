#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SdCardEntry {
    std::string name;
    bool isDirectory = false;
    uint64_t sizeBytes = 0;
};

class SdCardDriver {
public:
    static SdCardDriver& getInstance();

    bool begin();
    bool isMounted() const { return mounted_; }
    bool refreshStats();
    bool formatCard();

    uint64_t totalBytes() const { return total_bytes_; }
    uint64_t usedBytes() const { return used_bytes_; }
    uint8_t cardType() const { return card_type_; }
    const std::string& cardTypeString() const { return card_type_str_; }
    const std::string& lastError() const { return last_error_; }

    std::vector<SdCardEntry> listDirectory(const char* path, size_t max_entries = 32);
    bool removePath(const char* path);

    // SD mutex operations for thread safety
    bool acquireSdMutex(TickType_t timeout_ms = portMAX_DELAY);
    bool acquireSdMutexPriority(TickType_t timeout_ms = 0); // For timeshift priority
    void releaseSdMutex();

private:
    SdCardDriver();
    ~SdCardDriver();
    void updateCardTypeString();
    bool ensureMounted();
    bool deleteRecursive(const char* path);
    std::string buildChildPath(const char* parent, const char* child) const;

    SemaphoreHandle_t sd_mutex_ = nullptr;
    bool pins_configured_ = false;
    bool mounted_ = false;
    uint64_t total_bytes_ = 0;
    uint64_t used_bytes_ = 0;
    uint8_t card_type_ = 0;
    std::string card_type_str_;
    std::string last_error_;
};

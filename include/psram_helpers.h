#ifndef PSRAM_HELPERS_H
#define PSRAM_HELPERS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <memory>

// PSRAM-aware malloc with fallback to DRAM
static inline void* psram_malloc(size_t size) {
    if (!psramFound()) return malloc(size);
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return ptr ? ptr : malloc(size);
}

// PSRAM-aware free
static inline void psram_free(void* ptr) {
    if (ptr) {
        heap_caps_free(ptr);  // Safe for both
    }
}

// PSRAM JsonDocument allocator (for ArduinoJson)
// Note: DynamicJsonDocument(capacity) is deprecated upstream; keep for simplicity here.
template<size_t capacity>
static inline std::unique_ptr<DynamicJsonDocument, void (*)(DynamicJsonDocument*)> psram_json_document() {
    void* buffer = heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        return {nullptr, nullptr};
    }

    auto deleter = [](DynamicJsonDocument* doc) {
        if (doc) {
            doc->~DynamicJsonDocument();
            heap_caps_free(doc);
        }
    };

    return {new (buffer) DynamicJsonDocument(capacity), deleter};
}

#endif

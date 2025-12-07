#pragma once

#include <stddef.h>
#include <esp_heap_caps.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void *lvgl_psram_malloc(size_t size) {
    if(size == 0) {
        return NULL;
    }

    // Force PSRAM allocation only (no DRAM fallback) to save internal RAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        // If PSRAM allocation fails, log error but don't fallback to DRAM
        // This prevents silently consuming precious internal RAM
        return NULL;
    }
    return ptr;
}

static inline void *lvgl_psram_realloc(void *ptr, size_t size) {
    // Force PSRAM reallocation only (no DRAM fallback)
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static inline void lvgl_psram_free(void *ptr) {
    if(ptr == NULL) {
        return;
    }

    heap_caps_free(ptr);
}

#ifdef __cplusplus
}
#endif

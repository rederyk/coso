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

    return heap_caps_malloc_prefer(size, 2,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static inline void *lvgl_psram_realloc(void *ptr, size_t size) {
    return heap_caps_realloc_prefer(ptr, size, 2,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
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

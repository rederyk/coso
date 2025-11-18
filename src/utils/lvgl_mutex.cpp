#include "utils/lvgl_mutex.h"

static SemaphoreHandle_t s_lvgl_mutex = nullptr;

void lvgl_mutex_setup() {
    if (s_lvgl_mutex == nullptr) {
        s_lvgl_mutex = xSemaphoreCreateMutex();
    }
}

bool lvgl_mutex_lock(TickType_t timeout) {
    if (s_lvgl_mutex == nullptr) {
        return false;
    }
    return xSemaphoreTake(s_lvgl_mutex, timeout) == pdTRUE;
}

void lvgl_mutex_unlock() {
    if (s_lvgl_mutex != nullptr) {
        xSemaphoreGive(s_lvgl_mutex);
    }
}

SemaphoreHandle_t lvgl_mutex_get() {
    return s_lvgl_mutex;
}

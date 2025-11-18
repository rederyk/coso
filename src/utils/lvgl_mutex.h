#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

void lvgl_mutex_setup();
bool lvgl_mutex_lock(TickType_t timeout);
void lvgl_mutex_unlock();
SemaphoreHandle_t lvgl_mutex_get();

#pragma once

#include <freertos/FreeRTOS.h>
#include <stdint.h>

namespace TaskConfig {

// Core affinity
constexpr BaseType_t CORE_UI = 1;       // reserve Core 1 for LVGL/UI
constexpr BaseType_t CORE_WORK = 0;     // worker/comms core

// LVGL/UI task
constexpr uint32_t STACK_LVGL = 6144;
constexpr UBaseType_t PRIO_LVGL = 3;

// WiFi task
constexpr uint32_t STACK_WIFI = 4096;
constexpr UBaseType_t PRIO_WIFI = 1;

// HTTP server task
constexpr uint32_t STACK_HTTP = 6144;
constexpr UBaseType_t PRIO_HTTP = 2;

// BLE manager task
constexpr uint32_t STACK_BLE = 8192;
constexpr UBaseType_t PRIO_BLE = 5;

// RGB LED task
constexpr uint32_t STACK_LED = 2048;
constexpr UBaseType_t PRIO_LED = 0;

}  // namespace TaskConfig

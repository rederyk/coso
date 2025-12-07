#pragma once

#include <freertos/FreeRTOS.h>
#include <stdint.h>

namespace TaskConfig {

// Core affinity
constexpr BaseType_t CORE_UI = 1;       // reserve Core 1 for LVGL/UI
constexpr BaseType_t CORE_WORK = 0;     // worker/comms core

// LVGL/UI task
// Increased to 6 KB after stack canary trips with heavier UI workloads
constexpr uint32_t STACK_LVGL = 6144;
constexpr UBaseType_t PRIO_LVGL = 3;

// WiFi task
constexpr uint32_t STACK_WIFI = 3072;  // Reduced from 4KB to 3KB
constexpr UBaseType_t PRIO_WIFI = 1;

// HTTP server task
// Reduced from 24KB to 6KB - DynamicJsonDocument should be allocated on heap, not stack
// Further reduced to leave room for voice assistant in fragmented DRAM
constexpr uint32_t STACK_HTTP = 6144;  // 6 KB - JSON docs allocated on heap
constexpr UBaseType_t PRIO_HTTP = 2;

// BLE manager task
// Reduced from 8KB to 6KB to free DRAM
constexpr uint32_t STACK_BLE = 6144;  // 6 KB
constexpr UBaseType_t PRIO_BLE = 5;

// RGB LED task
constexpr uint32_t STACK_LED = 2048;
constexpr UBaseType_t PRIO_LED = 0;

// Async voice assistant worker (handles long-running LLM requests)
constexpr uint32_t VOICE_ASSISTANT_STACK_SIZE = 8192;
constexpr UBaseType_t VOICE_ASSISTANT_PRIORITY = 4;
constexpr BaseType_t VOICE_ASSISTANT_CORE = CORE_WORK;

}  // namespace TaskConfig

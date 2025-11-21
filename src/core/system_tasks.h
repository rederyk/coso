#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdint.h>
#include <functional>

enum class UiMessageType : uint8_t {
    None = 0,
    ApplyOrientation,
    Backlight,
    LedBrightness,
    ReloadApps,
    Callback,
    WifiStatus,
    BleStatus
};

struct UiMessage {
    UiMessageType type{UiMessageType::None};
    uint32_t value{0};
    void (*callback)(void*){nullptr};
    void* user_data{nullptr};
};

namespace SystemTasks {

void init();
bool postUiMessage(const UiMessage& message, TickType_t wait = 0);
bool postUiMessageFromISR(const UiMessage& message, BaseType_t* hp_task_woken = nullptr);
bool postUiCallback(void (*fn)(void*), void* ctx = nullptr, TickType_t wait = 0);
bool postUiCallbackFromISR(void (*fn)(void*), void* ctx = nullptr, BaseType_t* hp_task_woken = nullptr);
void drainUiQueue(const std::function<void(const UiMessage&)>& handler, TickType_t wait = 0);
QueueHandle_t uiQueue();

}  // namespace SystemTasks

#include "core/system_tasks.h"

#include "utils/logger.h"

namespace {
constexpr UBaseType_t UI_QUEUE_LENGTH = 10;
QueueHandle_t g_ui_queue = nullptr;
}  // namespace

void SystemTasks::init() {
    if (g_ui_queue) {
        return;
    }
    g_ui_queue = xQueueCreate(UI_QUEUE_LENGTH, sizeof(UiMessage));
    if (!g_ui_queue) {
        Logger::getInstance().error("[SystemTasks] Failed to allocate UI queue");
    }
}

bool SystemTasks::postUiMessage(const UiMessage& message, TickType_t wait) {
    if (!g_ui_queue) {
        return false;
    }
    return xQueueSend(g_ui_queue, &message, wait) == pdTRUE;
}

bool SystemTasks::postUiCallback(void (*fn)(void*), void* ctx, TickType_t wait) {
    if (!fn) {
        return false;
    }
    UiMessage msg{};
    msg.type = UiMessageType::Callback;
    msg.callback = fn;
    msg.user_data = ctx;
    return postUiMessage(msg, wait);
}

bool SystemTasks::postUiMessageFromISR(const UiMessage& message, BaseType_t* hp_task_woken) {
    if (!g_ui_queue) {
        return false;
    }
    return xQueueSendFromISR(g_ui_queue, &message, hp_task_woken) == pdTRUE;
}

bool SystemTasks::postUiCallbackFromISR(void (*fn)(void*), void* ctx, BaseType_t* hp_task_woken) {
    if (!fn) {
        return false;
    }
    UiMessage msg{};
    msg.type = UiMessageType::Callback;
    msg.callback = fn;
    msg.user_data = ctx;
    return postUiMessageFromISR(msg, hp_task_woken);
}

void SystemTasks::drainUiQueue(const std::function<void(const UiMessage&)>& handler, TickType_t wait) {
    if (!g_ui_queue || !handler) {
        return;
    }
    UiMessage msg{};
    while (xQueueReceive(g_ui_queue, &msg, wait) == pdTRUE) {
        handler(msg);
        wait = 0;  // only block the first receive
    }
}

QueueHandle_t SystemTasks::uiQueue() {
    return g_ui_queue;
}

#include "core/event_router.h"

EventRouter* EventRouter::instance = nullptr;

EventRouter::EventRouter() {
    mutex = xSemaphoreCreateMutex();
    Serial.println("[EventRouter] Initialized");
}

EventRouter::~EventRouter() {
    if (mutex) {
        vSemaphoreDelete(mutex);
        mutex = nullptr;
    }
}

EventRouter* EventRouter::getInstance() {
    if (instance == nullptr) {
        instance = new EventRouter();
    }
    return instance;
}

void EventRouter::subscribe(const char* event, EventCallback callback) {
    if (!event || !callback || !mutex) {
        return;
    }

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        listeners[event].push_back(callback);
        xSemaphoreGive(mutex);
    }
}

void EventRouter::publish(const char* event, void* data) {
    if (!event || !mutex) {
        return;
    }

    std::vector<EventCallback> callbacks;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        auto it = listeners.find(event);
        if (it != listeners.end()) {
            callbacks = it->second;
        }
        xSemaphoreGive(mutex);
    }

    for (auto& cb : callbacks) {
        cb(event, data);
    }
}

void EventRouter::unsubscribe(const char* event) {
    if (!event || !mutex) {
        return;
    }

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        listeners.erase(event);
        xSemaphoreGive(mutex);
    }
}

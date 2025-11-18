#pragma once

#include <Arduino.h>
#include <functional>
#include <map>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

using EventCallback = std::function<void(const char* event, void* data)>;

class EventRouter {
public:
    static EventRouter* getInstance();

    void subscribe(const char* event, EventCallback callback);
    void publish(const char* event, void* data = nullptr);
    void unsubscribe(const char* event);

private:
    EventRouter();
    ~EventRouter();

    static EventRouter* instance;
    std::map<String, std::vector<EventCallback>> listeners;
    SemaphoreHandle_t mutex = nullptr;
};

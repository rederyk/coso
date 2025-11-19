#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "Arduino.h"
#include "WiFi.h"

class WifiManager {
public:
    WifiManager();
    ~WifiManager();

    void init();
    void start();

private:
    static void wifi_task(void *pvParameters);
};

#endif // WIFI_MANAGER_H

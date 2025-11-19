#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "Arduino.h"

class BleManager {
public:
    BleManager();
    ~BleManager();

    void init();
    void start();

private:
    static void ble_task(void *pvParameters);
};

#endif // BLE_MANAGER_H

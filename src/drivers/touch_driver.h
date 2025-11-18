#pragma once

#include <lvgl.h>

// Pin touch per ESP32-S3 Freenove FNK0104 (versione con FT6336U)
#define TOUCH_I2C_SDA  16
#define TOUCH_I2C_SCL  15
#define TOUCH_RST      18
#define TOUCH_INT      17

// Calibrazione di default per il pannello FT6336.
// Modificare questi valori se le coordinate risultano ruotate/sfalsate.
#ifndef TOUCH_MAX_RAW_X
#define TOUCH_MAX_RAW_X 240
#endif

#ifndef TOUCH_MAX_RAW_Y
#define TOUCH_MAX_RAW_Y 320
#endif

#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY 1
#endif

#ifndef TOUCH_INVERT_X
#define TOUCH_INVERT_X 0
#endif

#ifndef TOUCH_INVERT_Y
#define TOUCH_INVERT_Y 1
#endif

void touch_driver_init();
bool touch_driver_available();
void touch_driver_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);

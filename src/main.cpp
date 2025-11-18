#include <Arduino.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

#include "core/screen_manager.h"
#include "screens/dashboard_screen.h"
#include "utils/lvgl_mutex.h"

static TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
// Ridotto a 20 righe per risparmiare memoria
static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * 20;
// Buffer statici invece di allocazione dinamica
static lv_color_t buf1[DRAW_BUF_PIXELS];

static void enableBacklight() {
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
}

static void tft_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, width, height);
    tft.pushColors((uint16_t*)color_p, width * height, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

static void lv_tick_handler(void*) {
    lv_tick_inc(1);
}

static void lvgl_task(void*) {
    while (true) {
        if (lvgl_mutex_lock(pdMS_TO_TICKS(100))) {
            lv_task_handler();
            lvgl_mutex_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== Freenove ESP32-S3 OS Dashboard ===");

    enableBacklight();
    tft.init();
    tft.setRotation(1);

    lv_init();

    // Single buffering con buffer statico
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, DRAW_BUF_PIXELS);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = tft_flush_cb;
    lv_disp_drv_register(&disp_drv);

    // Inizializza mutex PRIMA di usarlo
    lvgl_mutex_setup();

    const esp_timer_create_args_t tick_args = {
        .callback = &lv_tick_handler,
        .name = "lv_tick"
    };
    esp_timer_handle_t tick_handle;
    if (esp_timer_create(&tick_args, &tick_handle) == ESP_OK) {
        esp_timer_start_periodic(tick_handle, 1000);
    } else {
        Serial.println("[LVGL] Failed to create tick timer");
    }

    // Allocazione statica invece di dinamica
    static DashboardScreen dashboard;
    ScreenManager::getInstance()->pushScreen(&dashboard);

    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 6144, nullptr, 3, nullptr, 1);
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}

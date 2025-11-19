#include <Arduino.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

#include "core/app_manager.h"
#include "core/display_manager.h"
#include "core/settings_manager.h"
#include "drivers/touch_driver.h"
#include "screens/dashboard_screen.h"
#include "screens/settings_screen.h"
#include "screens/info_screen.h"
#include "screens/theme_settings_screen.h"
#include "utils/logger.h"
#include "utils/lvgl_mutex.h"

static TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
// Ridotto a 20 righe per risparmiare memoria
static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * 20;
// Buffer statico di fallback in RAM interna
static lv_color_t draw_buf_fallback[DRAW_BUF_PIXELS];
static lv_color_t* draw_buf_ptr = draw_buf_fallback;
static bool draw_buf_in_psram = false;
static constexpr const char* APP_VERSION = "0.5.0";

static void logSystemBanner();
static void logMemoryStats(const char* stage);
static void logLvglBufferInfo();
static void* allocatePsramBuffer(size_t size, const char* label);

static void logSystemBanner() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    auto& logger = Logger::getInstance();
    logger.info("\n=== Freenove ESP32-S3 OS Dashboard ===");
    logger.infof("[Build] %s %s | IDF %s", __DATE__, __TIME__, esp_get_idf_version());
    logger.infof("[Chip] ESP32-S3 rev %d | %d core(s) @ %d MHz",
                 chip_info.revision,
                 chip_info.cores,
                 getCpuFrequencyMhz());
    logger.infof("[Flash] %u MB QIO | [PSRAM] %u MB",
                 static_cast<unsigned>(ESP.getFlashChipSize() / (1024 * 1024)),
                 static_cast<unsigned>(ESP.getPsramSize() / (1024 * 1024)));
}

static void logMemoryStats(const char* stage) {
    const size_t dram_total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    const size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    const size_t dram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    const size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    auto& logger = Logger::getInstance();
    logger.infof("\n[Memory] Stage: %s", stage);
    logger.infof("  DRAM  free %7u / %7u bytes | largest block %7u",
                 static_cast<unsigned>(dram_free),
                 static_cast<unsigned>(dram_total),
                 static_cast<unsigned>(dram_largest));
    if (psram_total > 0) {
        logger.infof("  PSRAM free %7u / %7u bytes | largest block %7u",
                     static_cast<unsigned>(psram_free),
                     static_cast<unsigned>(psram_total),
                     static_cast<unsigned>(psram_largest));
    } else {
        logger.warn("  PSRAM not detected");
    }
}

static void logLvglBufferInfo() {
    const size_t bytes = DRAW_BUF_PIXELS * sizeof(lv_color_t);
    Logger::getInstance().infof("[LVGL] Draw buffer: %ld px (%u bytes) @ %p [%s]",
                                static_cast<long>(DRAW_BUF_PIXELS),
                                static_cast<unsigned>(bytes),
                                draw_buf_ptr,
                                draw_buf_in_psram ? "PSRAM" : "internal RAM");
}

static void* allocatePsramBuffer(size_t size, const char* label) {
    if (!psramFound()) {
        return nullptr;
    }
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr) {
        Logger::getInstance().infof("[PSRAM] %s allocated %u bytes @ %p",
                                    label,
                                    static_cast<unsigned>(size),
                                    ptr);
    } else {
        Logger::getInstance().warnf("[PSRAM] %s allocation FAILED (%u bytes)",
                                    label,
                                    static_cast<unsigned>(size));
    }
    return ptr;
}

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
    auto& logger = Logger::getInstance();
    logger.begin(115200);
    delay(200);
    logSystemBanner();
    logMemoryStats("Boot");

    SettingsManager& settings_mgr = SettingsManager::getInstance();
    bool initial_landscape = true;
    if (settings_mgr.begin()) {
        settings_mgr.setVersion(APP_VERSION);
        initial_landscape = settings_mgr.isLandscapeLayout();
    } else {
        logger.warn("[Settings] Initialization failed - persistent settings disabled");
    }

    if (psramFound()) {
        logger.info("✓ PSRAM detected and enabled!");
    } else {
        logger.warn("⚠ PSRAM not available - using internal RAM only");
    }

    touch_driver_init();
    bool has_touch = touch_driver_available();

    enableBacklight();
    tft.init();

    lv_init();
    logMemoryStats("After lv_init");

    const size_t draw_buf_bytes = DRAW_BUF_PIXELS * sizeof(lv_color_t);
    lv_color_t* allocated = static_cast<lv_color_t*>(allocatePsramBuffer(draw_buf_bytes, "LVGL draw buffer"));
    if (allocated) {
        draw_buf_ptr = allocated;
        draw_buf_in_psram = true;
    } else {
        draw_buf_ptr = draw_buf_fallback;
        draw_buf_in_psram = false;
        logger.warnf("[PSRAM] Using internal fallback buffer (%u bytes)",
                     static_cast<unsigned>(draw_buf_bytes));
    }

    // Single buffering con buffer in PSRAM (fallback statico se necessario)
    lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);
    logLvglBufferInfo();
    logMemoryStats("After draw buffer");

    DisplayManager& display_manager = DisplayManager::getInstance();
    display_manager.begin(&tft, &draw_buf, tft_flush_cb);
    display_manager.applyOrientation(initial_landscape, true);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_driver_read;
    lv_indev_drv_register(&indev_drv);

    logger.info("\n[INFO] Board: Freenove FNK0104 (2.8\" ILI9341 + FT6336)");
    logger.infof("[INFO] Touch controller: %s", has_touch ? "FT6336 detected" : "NOT detected - check wiring/pins");
    if (has_touch) {
        logger.info("[INFO] Capacitive touch enabled (LVGL pointer input).");
    } else {
        logger.warn("[WARN] Touch input registered but controller did not respond.");
        logger.warn("       Use Serial logs or src/utils/i2c_scanner.cpp to verify SDA/SCL pins.");
    }

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
        logger.error("[LVGL] Failed to create tick timer");
    }

    // Crea il display principale di LVGL
    lv_obj_t* screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Inizializza App Manager e crea le app
    AppManager* app_manager = AppManager::getInstance();
    app_manager->init(screen);
    app_manager->getDock()->onOrientationChanged(initial_landscape);

    // Crea le schermate (statiche)
    static DashboardScreen dashboard;
    static SettingsScreen settings;
    static InfoScreen info;
    static ThemeSettingsScreen theme_settings;

    // Registra le app nel dock
    app_manager->registerApp("dashboard", "🏠", "Home", &dashboard);
    app_manager->registerApp("settings", "⚙️", "Settings", &settings);
    app_manager->registerApp("theme", "🖌️", "Theme", &theme_settings);
    app_manager->registerApp("info", "ℹ️", "Info", &info);

    // Lancia dashboard come app iniziale
    app_manager->launchApp("dashboard");
    logMemoryStats("UI ready");

    settings_mgr.addListener([app_manager](SettingsManager::SettingKey key, const SettingsSnapshot& snapshot) {
        if (key != SettingsManager::SettingKey::LayoutOrientation) {
            return;
        }
        Logger::getInstance().infof("[Display] Orientation toggle requested: %s",
                                    snapshot.landscapeLayout ? "Landscape" : "Portrait");
        DisplayManager& display = DisplayManager::getInstance();
        display.applyOrientation(snapshot.landscapeLayout);
        app_manager->getDock()->onOrientationChanged(snapshot.landscapeLayout);
        app_manager->requestReload();
    });

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 6144, nullptr, 3, nullptr, 1);
    logMemoryStats("LVGL task started");
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}

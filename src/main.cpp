#include <Arduino.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <soc/rtc_cntl_reg.h>
#include <rom/rtc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

#include "core/app_manager.h"
#include "core/backlight_manager.h"
#include "core/display_manager.h"
#include "core/keyboard_manager.h"
#include "core/settings_manager.h"
#include "core/wifi_manager.h"
#include "core/ble_hid_manager.h"
#include "screens/ble_manager.h"
#include "drivers/touch_driver.h"
#include "drivers/sd_card_driver.h"
#include "drivers/rgb_led_driver.h"
#include "screens/dashboard_screen.h"
#include "screens/settings_screen.h"
#include "screens/wifi_settings_screen.h"
#include "screens/ble_settings_screen.h"
#include "screens/led_settings_screen.h"
#include "screens/developer_screen.h"
#include "screens/info_screen.h"
#include "screens/system_log_screen.h"
#include "screens/theme_settings_screen.h"
#include "screens/sd_explorer_screen.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/lvgl_mutex.h"

static TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
// Aumentato a 1/10 dell'altezza dello schermo per migliorare le performance
static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * (LV_VER_RES_MAX / 10);
// Rimosso il buffer statico, verrà allocato dinamicamente se necessario
static lv_color_t* draw_buf_ptr = nullptr;
static bool draw_buf_in_psram = false;
static constexpr const char* APP_VERSION = "0.5.0";

static WifiManager wifi_manager;

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
    BacklightManager& backlight = BacklightManager::getInstance();
    backlight.begin();
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
    uint8_t initial_brightness = 80;
    uint8_t initial_led_brightness = 50;

    // Check reset reason for diagnostics
    RESET_REASON reset_reason = rtc_get_reset_reason(0);
    const char* reset_reason_str = "UNKNOWN";
    switch (reset_reason) {
        case POWERON_RESET: reset_reason_str = "POWERON"; break;
        case DEEPSLEEP_RESET: reset_reason_str = "DEEP_SLEEP"; break;
        case TG0WDT_SYS_RESET: reset_reason_str = "TG0_WATCHDOG"; break;
        case TG1WDT_SYS_RESET: reset_reason_str = "TG1_WATCHDOG"; break;
        case RTCWDT_SYS_RESET: reset_reason_str = "RTC_WATCHDOG"; break;
        case INTRUSION_RESET: reset_reason_str = "INTRUSION"; break;
        case RTCWDT_CPU_RESET: reset_reason_str = "RTC_CPU_WATCHDOG"; break;
        case RTCWDT_BROWN_OUT_RESET: reset_reason_str = "BROWNOUT"; break;
        case RTCWDT_RTC_RESET: reset_reason_str = "RTC"; break;
        default: break;
    }
    logger.infof("[System] Reset reason: %s", reset_reason_str);

    if (settings_mgr.begin()) {
        settings_mgr.setVersion(APP_VERSION);

        // Increment boot count
        settings_mgr.incrementBootCount();
        logger.infof("[System] Boot count: %u", settings_mgr.getBootCount());

        // If brown-out detected, try to restore from SD backup
        if (reset_reason == RTCWDT_BROWN_OUT_RESET) {
            logger.warn("[System] Brown-out detected on last boot - checking for backup");
            if (settings_mgr.hasBackup()) {
                logger.info("[System] Attempting to restore settings from SD backup");
                if (settings_mgr.restoreFromSD()) {
                    logger.info("[System] Settings restored successfully from backup");
                }
            }
        }

        initial_landscape = settings_mgr.isLandscapeLayout();
        initial_brightness = settings_mgr.getBrightness();
        initial_led_brightness = settings_mgr.getLedBrightness();
    } else {
        logger.warn("[Settings] Initialization failed - persistent settings disabled");
    }

    // Initialize and start WiFi and BLE managers
    wifi_manager.init();
    wifi_manager.start();

    // Start BLE Manager (handles both server and client roles in a dedicated task)
    BleManager& ble_manager = BleManager::getInstance();
    ble_manager.start();

    // Start advertising
    ble_manager.startAdvertising();

    if (psramFound()) {
        logger.info("✓ PSRAM detected and enabled!");
    } else {
        logger.warn("⚠ PSRAM not available - using internal RAM only");
    }

    touch_driver_init();
    bool has_touch = touch_driver_available();

    tft.init();
    // TFT_eSPI reconfigures TFT_BL inside init(), so attach PWM afterwards to keep control.
    enableBacklight();
    BacklightManager::getInstance().setBrightness(initial_brightness);

    lv_init();
    logMemoryStats("After lv_init");

    const size_t draw_buf_bytes = DRAW_BUF_PIXELS * sizeof(lv_color_t);
    draw_buf_ptr = static_cast<lv_color_t*>(allocatePsramBuffer(draw_buf_bytes, "LVGL draw buffer"));
    if (draw_buf_ptr) {
        draw_buf_in_psram = true;
    } else {
        // Fallback: prova ad allocare in RAM interna (DRAM)
        draw_buf_in_psram = false;
        logger.warn("[PSRAM] Allocation failed. Attempting to allocate LVGL draw buffer in internal RAM...");
        draw_buf_ptr = static_cast<lv_color_t*>(heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (draw_buf_ptr) {
             logger.warnf("[DRAM] Using internal fallback buffer (%u bytes)", static_cast<unsigned>(draw_buf_bytes));
        } else {
            logger.error("[Memory] FATAL: Failed to allocate LVGL draw buffer in both PSRAM and internal RAM.");
            // A questo punto il sistema non può partire, blocchiamo l'esecuzione.
            while(1) { vTaskDelay(portMAX_DELAY); }
        }
    }

    // Single buffering con buffer in PSRAM (o DRAM come fallback)
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

    SdCardDriver& sd_driver = SdCardDriver::getInstance();
    if (!sd_driver.begin()) {
        logger.warn("[SD] No microSD detected at boot");
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

    // Initialize global keyboard manager
    KeyboardManager::getInstance().init(screen);
    logger.info("[KeyboardManager] Global keyboard initialized");

    // Inizializza App Manager e crea le app
    AppManager* app_manager = AppManager::getInstance();
    app_manager->init(screen);
    app_manager->getDock()->onOrientationChanged(initial_landscape);

    // Crea le schermate (statiche)
    static DashboardScreen dashboard;
    static SettingsScreen settings;
    static WifiSettingsScreen wifi_settings;
    static BleSettingsScreen ble_settings;
    static LedSettingsScreen led_settings;
    static DeveloperScreen developer;
    static InfoScreen info;
    static ThemeSettingsScreen theme_settings;
    static SystemLogScreen system_log;
    static SdExplorerScreen sd_explorer;

    // Registra le app nel dock
    app_manager->registerApp("dashboard", UI_SYMBOL_HOME, "Home", &dashboard);
    app_manager->registerApp("settings", UI_SYMBOL_SETTINGS, "Settings", &settings);
    app_manager->registerApp("theme", UI_SYMBOL_THEME, "Theme", &theme_settings);
    app_manager->registerApp("system_log", UI_SYMBOL_SYSLOG, "SysLog", &system_log);
    app_manager->registerApp("info", UI_SYMBOL_INFO, "Info", &info);
    app_manager->registerApp("sd_explorer", UI_SYMBOL_STORAGE, "SD Card", &sd_explorer);

    // Registra le schermate supplementari (non nel dock, accessibili solo da Settings)
    app_manager->registerHiddenApp("WiFiSettings", &wifi_settings);
    app_manager->registerHiddenApp("BleSettings", &ble_settings);
    app_manager->registerHiddenApp("LedSettings", &led_settings);
    app_manager->registerHiddenApp("Developer", &developer);

    // Lancia dashboard come app iniziale
    app_manager->launchApp("dashboard");
    logMemoryStats("UI ready");

    settings_mgr.addListener([app_manager](SettingsManager::SettingKey key, const SettingsSnapshot& snapshot) {
        if (key == SettingsManager::SettingKey::LayoutOrientation) {
            Logger::getInstance().infof("[Display] Orientation toggle requested: %s",
                                        snapshot.landscapeLayout ? "Landscape" : "Portrait");
            DisplayManager& display = DisplayManager::getInstance();
            display.applyOrientation(snapshot.landscapeLayout);
            app_manager->getDock()->onOrientationChanged(snapshot.landscapeLayout);
            app_manager->requestReload();
        } else if (key == SettingsManager::SettingKey::Brightness) {
            Logger::getInstance().infof("[Backlight] Brightness changed to %u%%", snapshot.brightness);
            BacklightManager::getInstance().setBrightness(snapshot.brightness);
        } else if (key == SettingsManager::SettingKey::LedBrightness) {
            Logger::getInstance().infof("[RGB LED] Brightness changed to %u%%", snapshot.ledBrightness);
            RgbLedManager::getInstance().setBrightness(snapshot.ledBrightness);
        }
    });

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 6144, nullptr, 3, nullptr, 1);
    logMemoryStats("LVGL task started");

    // Create auto-backup timer (every 30 minutes)
    static lv_timer_t* backup_timer = lv_timer_create([](lv_timer_t* t) {
        SettingsManager& settings = SettingsManager::getInstance();
        Logger& log = Logger::getInstance();

        log.info("[Settings] Performing periodic backup to SD card");
        if (settings.backupToSD()) {
            log.info("[Settings] Periodic backup completed successfully");
        } else {
            log.warn("[Settings] Periodic backup failed (SD card may not be present)");
        }
    }, 30 * 60 * 1000, nullptr);  // 30 minutes in milliseconds

    logger.info("[Settings] Auto-backup timer started (every 30 minutes)");

    // Inizializza RGB LED dopo che tutto il resto è pronto
    RgbLedManager& rgb_led = RgbLedManager::getInstance();
    if (rgb_led.begin(42)) {  // GPIO42 per il LED RGB integrato su Freenove ESP32-S3
        rgb_led.setBrightness(initial_led_brightness);
        rgb_led.setState(RgbLedManager::LedState::BOOT);
        logger.info("[RGB LED] Initialized with boot animation");
    } else {
        logger.warn("[RGB LED] Initialization failed");
    }
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}

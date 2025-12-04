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
#include "core/web_server_manager.h"
#include "core/system_tasks.h"
#include "core/ble_hid_manager.h"
#include "core/task_config.h"
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
#include "screens/ble_remote_screen.h"
#include "screens/ble_keyboard_screen.h"
#include "screens/ble_mouse_screen.h"
#include "screens/audio_player_screen.h"
#include "screens/web_radio_screen.h"
#include "screens/audio_effects_screen.h"
#include "screens/microphone_test_screen.h"
#include "screens/voice_assistant_settings_screen.h"
#include "core/audio_manager.h"
#include "core/microphone_manager.h"
#include "core/conversation_buffer.h"
// #include "core/voice_assistant.h"  // Temporarily disabled due to include issues
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/lvgl_mutex.h"

// ========== CONFIGURAZIONE BUFFER LVGL ==========
// Modifica LVGL_BUFFER_MODE in platformio.ini per testare diverse modalità:
//   0 = PSRAM Single Buffer (baseline, bassa performance DMA)
//   1 = DRAM Single Buffer (RACCOMANDATO: ottimo bilanciamento performance/memoria)
//   2 = DRAM Double Buffer (massime performance, usa 30 KB RAM)
// Default se non specificato: DRAM Single (modo 1)
#ifndef LVGL_BUFFER_MODE
  #define LVGL_BUFFER_MODE 1
#endif

#if LVGL_BUFFER_MODE < 0 || LVGL_BUFFER_MODE > 2
  #error "LVGL_BUFFER_MODE deve essere 0, 1 o 2"
#endif

static TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
// Aumentato a 1/10 dell'altezza dello schermo per migliorare le performance
static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * (LV_VER_RES_MAX / 10);

// Buffer LVGL allocati dinamicamente in base a LVGL_BUFFER_MODE
static lv_color_t* draw_buf_ptr = nullptr;
#if LVGL_BUFFER_MODE == 2
  static lv_color_t* draw_buf_ptr2 = nullptr;  // Secondo buffer per double buffering
#endif

static constexpr const char* APP_VERSION = "0.5.0";

static WifiManager wifi_manager;

static void logSystemBanner();
static void logMemoryStats(const char* stage);
static void logLvglBufferInfo();
static void* allocatePsramBuffer(size_t size, const char* label);
static void handleUiMessage(const UiMessage& message);

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
    const size_t dram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    const size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t dram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
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
    auto& logger = Logger::getInstance();

    // Determina la modalità di buffering
    const char* mode_name = nullptr;
    const char* location = nullptr;

    #if LVGL_BUFFER_MODE == 0
        mode_name = "PSRAM Single";
        location = "PSRAM";
    #elif LVGL_BUFFER_MODE == 1
        mode_name = "DRAM Single";
        location = "Internal RAM";
    #elif LVGL_BUFFER_MODE == 2
        mode_name = "DRAM Double";
        location = "Internal RAM";
    #endif

    logger.infof("[LVGL] Buffer mode: %s", mode_name);
    logger.infof("[LVGL] Buffer 1: %ld px (%u bytes) @ %p [%s]",
                 static_cast<long>(DRAW_BUF_PIXELS),
                 static_cast<unsigned>(bytes),
                 draw_buf_ptr,
                 location);

    #if LVGL_BUFFER_MODE == 2
        logger.infof("[LVGL] Buffer 2: %ld px (%u bytes) @ %p [%s]",
                     static_cast<long>(DRAW_BUF_PIXELS),
                     static_cast<unsigned>(bytes),
                     draw_buf_ptr2,
                     location);
        logger.infof("[LVGL] Total RAM used: %u bytes", static_cast<unsigned>(bytes * 2));
    #endif
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
            // Process UI messages while holding the LVGL mutex
            SystemTasks::drainUiQueue(handleUiMessage, pdMS_TO_TICKS(1));
            lv_timer_handler();
            lvgl_mutex_unlock();
        }
        // 5ms delay for smoother UI (up to ~200 FPS theoretical max)
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void handleUiMessage(const UiMessage& message) {
    AppManager* app_manager = AppManager::getInstance();
    switch (message.type) {
        case UiMessageType::ApplyOrientation: {
            bool landscape = message.value != 0;
            DisplayManager& display = DisplayManager::getInstance();
            display.applyOrientation(landscape);
            if (app_manager) {
                app_manager->getDock()->onOrientationChanged(landscape);
                app_manager->requestReload();
            }
            Logger::getInstance().infof("[Display] Orientation applied via UI queue: %s",
                                        landscape ? "Landscape" : "Portrait");
            break;
        }
        case UiMessageType::Backlight:
            Logger::getInstance().infof("[Backlight] Brightness changed to %u%%", static_cast<unsigned>(message.value));
            BacklightManager::getInstance().setBrightness(static_cast<uint8_t>(message.value));
            break;
        case UiMessageType::LedBrightness:
            Logger::getInstance().infof("[RGB LED] Brightness changed to %u%%", static_cast<unsigned>(message.value));
            RgbLedManager::getInstance().setBrightness(static_cast<uint8_t>(message.value));
            break;
        case UiMessageType::ReloadApps:
            if (app_manager) {
                app_manager->requestReload();
            }
            break;
        case UiMessageType::Callback:
            if (message.callback) {
                message.callback(message.user_data);
            }
            break;
        case UiMessageType::WifiStatus:
            Logger::getInstance().infof("[WiFi] Status update: %s",
                                        message.value ? "Connected" : "Disconnected");
            // La UI può usare questo evento per aggiornare widget di stato, icone, etc.
            break;
        case UiMessageType::BleStatus:
            Logger::getInstance().infof("[BLE] Status update: %u", static_cast<unsigned>(message.value));
            // La UI può usare questo evento per aggiornare widget di stato, icone, etc.
            break;
        default:
            break;
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

    if (!ConversationBuffer::getInstance().begin()) {
        logger.warn("[VoiceAssistant] Conversation buffer unavailable");
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

    // Initialize Audio Manager
    logger.info("[Audio] Initializing audio manager");
    AudioManager& audio_manager = AudioManager::getInstance();
    audio_manager.begin();
    logger.info("[Audio] Audio manager ready");

    // Initialize Microphone Manager
    logger.info("[MicMgr] Initializing microphone manager");
    MicrophoneManager& mic_manager = MicrophoneManager::getInstance();
    if (mic_manager.begin()) {
        logger.info("[MicMgr] Microphone manager ready");
    } else {
        logger.warn("[MicMgr] Microphone manager initialization failed");
    }

    // Initialize Voice Assistant (if enabled in settings)
    // TODO: Enable after fixing include issues
    // logger.info("[VoiceAssistant] Initializing voice assistant");
    // VoiceAssistant& voice_assistant = VoiceAssistant::getInstance();
    // if (voice_assistant.begin()) {
    //     logger.info("[VoiceAssistant] Voice assistant initialized successfully");
    // } else {
    //     logger.warn("[VoiceAssistant] Voice assistant initialization failed or disabled");
    // }

    touch_driver_init();
    bool has_touch = touch_driver_available();

    tft.init();
    // TFT_eSPI reconfigures TFT_BL inside init(), so attach PWM afterwards to keep control.
    enableBacklight();
    BacklightManager::getInstance().setBrightness(initial_brightness);

    // Initialize RGB LED early for error indication
    RgbLedManager& rgb_led = RgbLedManager::getInstance();
    rgb_led.begin(42);
    rgb_led.setBrightness(initial_led_brightness);

    lv_init();
    logMemoryStats("After lv_init");

    const size_t draw_buf_bytes = DRAW_BUF_PIXELS * sizeof(lv_color_t);

    // ========== ALLOCAZIONE BUFFER IN BASE A LVGL_BUFFER_MODE ==========

    #if LVGL_BUFFER_MODE == 0
        // Modalità 0: PSRAM Single Buffer (baseline)
        logger.info("[LVGL] Allocating PSRAM single buffer...");
        draw_buf_ptr = static_cast<lv_color_t*>(allocatePsramBuffer(draw_buf_bytes, "LVGL buffer"));
        if (!draw_buf_ptr) {
            logger.error("[LVGL] FATAL: Failed to allocate PSRAM buffer");
            rgb_led.setState(RgbLedManager::LedState::ERROR);
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_restart();
        }
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);

    #elif LVGL_BUFFER_MODE == 1
        // Modalità 1: DRAM Single Buffer (RACCOMANDATO)
        logger.info("[LVGL] Allocating DRAM single buffer...");
        draw_buf_ptr = static_cast<lv_color_t*>(
            heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
        );
        if (!draw_buf_ptr) {
            logger.error("[LVGL] DRAM buffer allocation failed - attempting fallback to PSRAM");
            logger.warnf("[LVGL] Requested %u bytes DRAM, largest block: %u bytes",
                        static_cast<unsigned>(draw_buf_bytes),
                        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
            // Fallback a PSRAM single buffer
            draw_buf_ptr = static_cast<lv_color_t*>(allocatePsramBuffer(draw_buf_bytes, "LVGL buffer fallback"));
            if (!draw_buf_ptr) {
                logger.error("[LVGL] FATAL: Both DRAM and PSRAM buffer allocation failed");
                rgb_led.setState(RgbLedManager::LedState::ERROR);
                vTaskDelay(pdMS_TO_TICKS(5000));
                esp_restart();
            }
        }
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);

    #elif LVGL_BUFFER_MODE == 2
        // Modalità 2: DRAM Double Buffer (massime performance)
        logger.info("[LVGL] Allocating DRAM double buffer...");
        draw_buf_ptr = static_cast<lv_color_t*>(
            heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
        );
        draw_buf_ptr2 = static_cast<lv_color_t*>(
            heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
        );
        if (!draw_buf_ptr || !draw_buf_ptr2) {
            logger.error("[LVGL] DRAM double buffer allocation failed - attempting fallback");
            logger.warnf("[LVGL] Requested 2x %u bytes DRAM, largest block: %u bytes",
                        static_cast<unsigned>(draw_buf_bytes),
                        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
            // Libera eventuali buffer allocati
            if (draw_buf_ptr) heap_caps_free(draw_buf_ptr);
            if (draw_buf_ptr2) heap_caps_free(draw_buf_ptr2);
            draw_buf_ptr = nullptr;
            draw_buf_ptr2 = nullptr;

            // Fallback a DRAM single buffer
            draw_buf_ptr = static_cast<lv_color_t*>(
                heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
            );
            if (!draw_buf_ptr) {
                logger.error("[LVGL] DRAM single buffer fallback failed - trying PSRAM");
                // Fallback finale a PSRAM single buffer
                draw_buf_ptr = static_cast<lv_color_t*>(allocatePsramBuffer(draw_buf_bytes, "LVGL buffer fallback"));
                if (!draw_buf_ptr) {
                    logger.error("[LVGL] FATAL: Both DRAM and PSRAM buffer allocation failed");
                    rgb_led.setState(RgbLedManager::LedState::ERROR);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    esp_restart();
                }
            }
        }
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, draw_buf_ptr2, DRAW_BUF_PIXELS);
    #endif

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
    SystemTasks::init();

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
    static BleRemoteScreen ble_remote;
    static BleKeyboardScreen ble_keyboard_screen;
    static BleMouseScreen ble_mouse_screen;
    static AudioPlayerScreen audio_player;
    static WebRadioScreen web_radio;
    static AudioEffectsScreen audio_effects;
    static MicrophoneTestScreen microphone_test;
    static VoiceAssistantSettingsScreen voice_assistant_settings;

    // Registra le app nel dock
    app_manager->registerApp("dashboard", UI_SYMBOL_HOME, "Home", &dashboard);
    app_manager->registerApp("settings", UI_SYMBOL_SETTINGS, "Settings", &settings);
    app_manager->registerApp("theme", UI_SYMBOL_THEME, "Theme", &theme_settings);
    app_manager->registerApp("ble_remote", UI_SYMBOL_BLUETOOTH, "BLE Remote", &ble_remote);
    app_manager->registerApp("system_log", UI_SYMBOL_SYSLOG, "SysLog", &system_log);
    app_manager->registerApp("info", UI_SYMBOL_INFO, "Info", &info);
    app_manager->registerApp("sd_explorer", UI_SYMBOL_STORAGE, "SD Card", &sd_explorer);
    app_manager->registerApp("audio_player", LV_SYMBOL_AUDIO, "Music", &audio_player);
    app_manager->registerApp("web_radio", LV_SYMBOL_WIFI, "Radio", &web_radio);
    app_manager->registerApp("audio_effects", LV_SYMBOL_SETTINGS, "FX", &audio_effects);
    app_manager->registerApp("microphone_test", LV_SYMBOL_AUDIO, "Recorder", &microphone_test);

    // Registra le schermate supplementari (non nel dock, accessibili solo da Settings)
    app_manager->registerHiddenApp("WiFiSettings", &wifi_settings);
    app_manager->registerHiddenApp("BleSettings", &ble_settings);
    app_manager->registerHiddenApp("LedSettings", &led_settings);
    app_manager->registerHiddenApp("Developer", &developer);
    app_manager->registerHiddenApp("ble_keyboard", &ble_keyboard_screen);
    app_manager->registerHiddenApp("ble_mouse", &ble_mouse_screen);
    app_manager->registerHiddenApp("VoiceAssistantSettings", &voice_assistant_settings);

    // Lancia dashboard come app iniziale
    app_manager->launchApp("dashboard");
    logMemoryStats("UI ready");

    settings_mgr.addListener([](SettingsManager::SettingKey key, const SettingsSnapshot& snapshot) {
        UiMessage msg{};
        Logger& log = Logger::getInstance();

        switch (key) {
            case SettingsManager::SettingKey::LayoutOrientation:
                log.infof("[Display] Orientation toggle requested: %s",
                          snapshot.landscapeLayout ? "Landscape" : "Portrait");
                msg.type = UiMessageType::ApplyOrientation;
                msg.value = snapshot.landscapeLayout ? 1u : 0u;
                break;
            case SettingsManager::SettingKey::Brightness:
                msg.type = UiMessageType::Backlight;
                msg.value = snapshot.brightness;
                break;
            case SettingsManager::SettingKey::LedBrightness:
                msg.type = UiMessageType::LedBrightness;
                msg.value = snapshot.ledBrightness;
                break;
            default:
                break;
        }

        if (msg.type != UiMessageType::None) {
            if (!SystemTasks::postUiMessage(msg, pdMS_TO_TICKS(50))) {
                log.warn("[System] Failed to enqueue UI message (queue full)");
            }
        }
    });

    xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl",
        TaskConfig::STACK_LVGL,
        nullptr,
        TaskConfig::PRIO_LVGL,
        nullptr,
        TaskConfig::CORE_UI);
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

    // Create memory logging timer (every 60 seconds)
    static lv_timer_t* memory_timer = lv_timer_create([](lv_timer_t* t) {
        logMemoryStats("Periodic");
    }, 60 * 1000, nullptr);  // 60 seconds in milliseconds

    logger.info("[System] Memory logging started (every 60 seconds)");


}

void loop() {
    // Audio manager tick (required for playback)
    AudioManager::getInstance().tick();

    // Yield to FreeRTOS scheduler
    vTaskDelay(pdMS_TO_TICKS(20));
}

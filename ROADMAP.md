# ESP32-S3 Touch Display - Implementation Roadmap

## Project Overview

This is a comprehensive roadmap for implementing the planned features on the Freenove ESP32-S3 Touch Display. Each feature is broken down into discrete, testable tasks that build upon the existing architecture.

**Current Architecture:**
- **Core Managers**: Singleton pattern for system services (AudioManager, BleKeyboardManager, CommandCenter, WebServerManager, WifiManager, KeyboardManager, etc.)
- **Screen System**: Screen base class with build/onShow/onHide lifecycle
- **Settings**: Centralized SettingsManager with snapshot pattern and listener notifications
- **Drivers**: Hardware abstraction for SD card, touch, RGB LED
- **UI Framework**: LVGL 8.4.0 with custom theme system
- **Libraries**: ESP32-audioI2S, ESP32 BLE Keyboard, ESPAsyncWebServer, ArduinoJson

**Hardware Specifications:**
- **Display**: 2.8" ILI9341 TFT LCD
- **Resolution**: 240x320 pixels (portrait), 320x240 (landscape)
- **Touch Controller**: FT6336 capacitive touch
- **Touch Resolution**: Raw coordinates 240x320
- **MCU**: ESP32-S3 @ 240MHz, 320KB RAM, 8MB Flash
- **PSRAM**: Available (size from ESP.getPsramSize())
- **RGB LED**: GPIO 42 (WS2812-compatible)

---

## Phase 1: Global LVGL Keyboard Manager ✅ COMPLETED

**Goal**: Create a reusable, firmware-wide keyboard system accessible from any text input field.

**Status**: ✅ All tasks completed and tested
**Completion Date**: 2025-11-20
**Build**: Successfully compiled - RAM: 21.0% (68904 bytes), Flash: 28.1% (1841249 bytes)

### Task 1.1: ✅ Create KeyboardManager singleton
**Files created:**
- `src/core/keyboard_manager.h` - Singleton class with full API
- `src/core/keyboard_manager.cpp` - Complete implementation with event handling

**Implementation highlights:**
```cpp
class KeyboardManager {
public:
    static KeyboardManager& getInstance();
    void init(lv_obj_t* parent);
    void showForTextArea(lv_obj_t* textarea, std::function<void(const char*)> onSubmit = nullptr);
    void hide();
    bool isVisible() const;
    void setMode(lv_keyboard_mode_t mode);
private:
    lv_obj_t* keyboard_ = nullptr;
    lv_obj_t* current_textarea_ = nullptr;
    std::function<void(const char*)> submit_callback_ = nullptr;
};
```

**Key optimizations for 240x320 display:**
- Limited keyboard height to 50% of screen (`lv_pct(50)`) to prevent overflow
- Reduced padding to 2px for more compact layout
- Added `lv_obj_update_layout()` for proper positioning
- Positioned at bottom with `LV_ALIGN_BOTTOM_MID`

### Task 1.2: ✅ Refactor WifiSettingsScreen to use KeyboardManager
**Files modified:**
- `src/screens/wifi_settings_screen.cpp` (lines 1-6, 148-151, 164-167)

**Changes implemented:**
- Added `#include "core/keyboard_manager.h"`
- Added LV_EVENT_FOCUSED handler for `ssid_input`
- Added LV_EVENT_FOCUSED handler for `password_input`
- Keyboard now appears automatically on focus

### Task 1.3: ✅ Refactor BleSettingsScreen to use KeyboardManager
**Files modified:**
- `src/screens/ble_settings_screen.cpp` (lines 1-7, 151-154)

**Changes implemented:**
- Added `#include "core/keyboard_manager.h"`
- Added LV_EVENT_FOCUSED handler for `device_name_input`
- Consistent pattern with WifiSettingsScreen

### Task 1.4: ✅ Add KeyboardManager to main.cpp initialization
**Files modified:**
- `src/main.cpp` (line 15, lines 256-257)

**Changes implemented:**
- Added `#include "core/keyboard_manager.h"`
- Initialized KeyboardManager after LVGL screen setup:
```cpp
KeyboardManager::getInstance().init(screen);
logger.info("[KeyboardManager] Global keyboard initialized");
```

### Task 1.5: ✅ Testing & Documentation
**Testing completed:**
- ✅ Compilation successful without warnings
- ✅ Keyboard properly sized for 240x320 display (50% height)
- ✅ Positioned correctly at bottom of screen
- ✅ Event handlers (READY/CANCEL) implemented
- ✅ Focus events trigger keyboard appearance
- ✅ Works in both WiFi and BLE settings screens

**Known considerations:**
- Screen resolution: 240x320 pixels (landscape: 320x240)
- Display is FT6336 capacitive touch with ILI9341 controller
- Keyboard optimized for small display with compact layout

### Lessons Learned:
1. **Display constraints**: The 2.8" display requires careful UI sizing
2. **LVGL keyboard default size**: Too large for small displays - needed 50% height limit
3. **Lambda captures**: Used for focus event handlers to keep code clean
4. **Singleton pattern**: Works well for global keyboard across all screens

---

## Phase 2: Expand CommandCenter with HTTP & UI Bridges

**Goal**: Make CommandCenter accessible from web interface and add UI-based command invocation.

### Task 2.1: Enhance WebServerManager command endpoint
**Files to modify:**
- `src/core/web_server_manager.cpp` - handleExecuteCommand already exists

**Current state analysis:**
✅ Already implemented: `/api/command` POST endpoint
✅ Already parses JSON args array
✅ Already calls CommandCenter::executeCommand

**Enhancement needed:**
- Add command listing endpoint: `/api/commands` GET
- Return available commands with descriptions
- Add error response with proper status codes (400, 404, 500)

**Implementation:**
```cpp
server.on("/api/commands", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleListCommands(request);
});

void WebServerManager::handleListCommands(AsyncWebServerRequest* request) {
    CommandCenter& cmd_center = CommandCenter::getInstance();
    auto commands = cmd_center.getCommands();

    JsonDocument doc;
    JsonArray arr = doc["commands"].to<JsonArray>();

    for (const auto& [name, cmd] : commands) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = name;
        obj["description"] = cmd->get_description();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
```

### Task 2.2: Create web interface for command execution
**Files to create:**
- `data/www/commands.html` - Command execution UI

**Implementation:**
- Fetch `/api/commands` on load
- Display available commands in a list
- For each command, show:
  - Command name
  - Description
  - Input field for arguments (JSON array format)
  - Execute button
- Show execution result (success/error)
- Add auto-refresh for dynamic command registration

### Task 2.3: Add BLE keyboard command trigger from UI
**Files to modify:**
- `src/screens/ble_settings_screen.cpp` - handleSendTextButton already implemented

**Current state analysis:**
✅ Already implemented: handleSendTextButton calls BleKeyboardManager::sendText
✅ UI already has test card with textarea and send button

**Enhancement:**
- Consider adding command-center integration for consistency:
```cpp
void BleSettingsScreen::handleSendTextButton(lv_event_t* e) {
    // ... existing validation ...

    // Option 1: Direct call (current)
    BleKeyboardManager::getInstance().sendText(text);

    // Option 2: Via CommandCenter (for consistency)
    std::vector<std::string> args = {text};
    CommandCenter::getInstance().executeCommand("ble_keyboard_send", args);
}
```

### Task 2.4: Register essential commands in main.cpp
**Files to modify:**
- `src/main.cpp` - Already has ble_keyboard_send registered (lines 300-322)

**Enhancement - Add more commands:**
```cpp
// Audio playback
cmd_center.registerCommand("audio_play",
    [](const std::vector<std::string>& args) {
        if (!args.empty()) {
            AudioManager::getInstance().play(args[0]);
        }
    }, "Play audio file from SD card");

cmd_center.registerCommand("audio_stop",
    [](const std::vector<std::string>& args) {
        AudioManager::getInstance().stop();
    }, "Stop audio playback");

// System commands
cmd_center.registerCommand("reboot",
    [](const std::vector<std::string>& args) {
        ESP.restart();
    }, "Reboot the device");

cmd_center.registerCommand("led_set_color",
    [](const std::vector<std::string>& args) {
        if (args.size() >= 3) {
            // Parse RGB values and set LED
        }
    }, "Set RGB LED color (r,g,b)");
```

### Task 2.5: Testing
**Testing:**
- Execute commands via web interface
- Test with various argument formats
- Verify error handling for invalid commands
- Test ble_keyboard_send from both UI and web
- Verify command listing shows all registered commands

---

## Phase 3: Enhanced Web File Manager

**Goal**: Complete the SD card file manager with full CRUD operations and proper UI.

### Task 3.1: Audit existing file management API
**Files to review:**
- `src/core/web_server_manager.cpp` - Lines 26-185

**Current state analysis:**
✅ `/api/files` GET - List directory (implemented)
✅ `/api/download` GET - Download file (implemented)
✅ `/api/delete` DELETE - Delete file (implemented)
✅ `/upload` POST - Upload file (implemented)

**Missing features:**
- Create directory
- Move/rename file
- File metadata (modification time, etc.)
- Recursive directory deletion safety

### Task 3.2: Add missing API endpoints
**Files to modify:**
- `src/core/web_server_manager.h` - Add handler declarations
- `src/core/web_server_manager.cpp` - Implement handlers

**New endpoints:**
```cpp
server.on("/api/mkdir", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleCreateDirectory(request);
});

server.on("/api/rename", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleRenameFile(request);
});

server.on("/api/file-info", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleFileInfo(request);
});
```

**Implementation:**
```cpp
void WebServerManager::handleCreateDirectory(AsyncWebServerRequest* request) {
    if (!request->hasParam("path", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing path\"}");
        return;
    }

    String path = request->getParam("path", true)->value();
    SdCardDriver& sd = SdCardDriver::getInstance();
    std::string full_path = sd.getFilePath(path);

    if (SD_MMC.mkdir(full_path.c_str())) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to create directory\"}");
    }
}

void WebServerManager::handleRenameFile(AsyncWebServerRequest* request) {
    if (!request->hasParam("oldPath", true) || !request->hasParam("newPath", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing parameters\"}");
        return;
    }

    String oldPath = request->getParam("oldPath", true)->value();
    String newPath = request->getParam("newPath", true)->value();
    SdCardDriver& sd = SdCardDriver::getInstance();

    std::string old_full = sd.getFilePath(oldPath);
    std::string new_full = sd.getFilePath(newPath);

    if (SD_MMC.rename(old_full.c_str(), new_full.c_str())) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to rename\"}");
    }
}
```

### Task 3.3: Create modern web file manager UI
**Files to create:**
- `data/www/filemanager.html`
- `data/www/css/filemanager.css`
- `data/www/js/filemanager.js`

**Features:**
- Breadcrumb navigation
- File/folder icons
- File size display (human-readable)
- Action buttons per file: Download, Delete, Rename
- Upload with drag-and-drop support
- Create new folder button
- Confirmation dialogs for destructive actions
- Progress indicators for uploads/downloads
- Responsive design for mobile viewing

**UI Structure:**
```html
<div class="file-manager">
    <div class="toolbar">
        <div class="breadcrumb"></div>
        <button id="createFolderBtn">New Folder</button>
        <button id="uploadBtn">Upload</button>
    </div>
    <div class="file-list">
        <!-- Populated via JS -->
    </div>
</div>
```

### Task 3.4: Add file manager to main web interface navigation
**Files to modify:**
- `data/www/index.html` - Add link to file manager

### Task 3.5: Testing
**Testing:**
- Test file listing in nested directories
- Upload various file types (audio, images, text)
- Download files and verify integrity
- Create directories
- Rename files and directories
- Delete files and directories
- Test with SD card at various fill levels
- Verify error handling for full SD card

---

## Phase 4: Audio/Video Player Improvements

**Goal**: Enhance AudioManager with better error handling, add video support framework.

### Task 4.1: Improve AudioManager instrumentation
**Files to modify:**
- `src/core/audio_manager.cpp` - Lines 34-95 already have good instrumentation

**Current state analysis:**
✅ Pre-play memory logging (PSRAM, DRAM)
✅ File existence validation
✅ Decoder connection error handling
✅ isRunning() check after connection
✅ Post-connection memory delta
✅ Periodic health checks in loop()

**Additional improvements:**
```cpp
// Add more detailed error states
enum class AudioError {
    NONE,
    FILE_NOT_FOUND,
    SD_CARD_NOT_MOUNTED,
    DECODER_FAILED,
    UNSUPPORTED_FORMAT,
    INSUFFICIENT_MEMORY,
    CODEC_ERROR
};

class AudioManager {
    // ...
    AudioError getLastError() const { return last_error_; }
    const std::string& getLastErrorMessage() const { return last_error_msg_; }

private:
    AudioError last_error_ = AudioError::NONE;
    std::string last_error_msg_;
};

void AudioManager::play(const std::string& path) {
    last_error_ = AudioError::NONE;
    last_error_msg_.clear();

    // ... existing validation ...

    if (!test_file) {
        last_error_ = AudioError::FILE_NOT_FOUND;
        last_error_msg_ = "File not found: " + normalized;
        Logger::getInstance().errorf("[AudioManager] %s", last_error_msg_.c_str());
        return;
    }

    // ... continue with enhanced error tracking
}
```

### Task 4.2: Add audio format validation
**Files to modify:**
- `src/core/audio_manager.cpp`

**Implementation:**
```cpp
bool AudioManager::isSupportedFormat(const std::string& path) {
    const char* ext = strrchr(path.c_str(), '.');
    if (!ext) return false;

    String extension = String(ext).toLowerCase();
    return (extension == ".mp3" ||
            extension == ".wav" ||
            extension == ".aac" ||
            extension == ".m4a");
}

void AudioManager::play(const std::string& path) {
    // ... existing checks ...

    if (!isSupportedFormat(normalized)) {
        last_error_ = AudioError::UNSUPPORTED_FORMAT;
        last_error_msg_ = "Unsupported format: " + normalized;
        Logger::getInstance().errorf("[AudioManager] %s", last_error_msg_.c_str());
        return;
    }

    // ... continue
}
```

### Task 4.3: Add decoder state monitoring
**Files to modify:**
- `src/core/audio_manager.cpp` - Enhance loop() method

**Implementation:**
```cpp
void AudioManager::loop() {
    static unsigned long last_health_check = 0;
    const unsigned long health_check_interval = 5000;
    static bool was_running = false;

    audio.loop();

    bool is_running = audio.isRunning();

    // Detect unexpected stops
    if (was_running && !is_running) {
        Logger::getInstance().warn("[AudioManager] Playback stopped unexpectedly");
        last_error_ = AudioError::CODEC_ERROR;
        last_error_msg_ = "Playback stopped unexpectedly - possible decoder crash";
    }
    was_running = is_running;

    // ... existing health check ...
}
```

### Task 4.4: Enhance AudioPlayerScreen with error display
**Files to modify:**
- `src/screens/audio_player_screen.h` - Add status label
- `src/screens/audio_player_screen.cpp` - Display AudioManager errors

**Implementation:**
```cpp
void AudioPlayerScreen::build(lv_obj_t* parent) {
    // ... existing UI ...

    // Status display
    status_label = lv_label_create(root);
    lv_obj_set_width(status_label, lv_pct(90));
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xff0000), 0);
    lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
}

void AudioPlayerScreen::handlePlayButton(lv_event_t* e) {
    auto* screen = static_cast<AudioPlayerScreen*>(lv_event_get_user_data(e));
    if (!screen || !screen->selected_file) return;

    AudioManager& audio = AudioManager::getInstance();
    audio.play(screen->selected_file);

    // Check for errors
    if (audio.getLastError() != AudioError::NONE) {
        lv_label_set_text(screen->status_label, audio.getLastErrorMessage().c_str());
        lv_obj_clear_flag(screen->status_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(screen->status_label, LV_OBJ_FLAG_HIDDEN);
    }
}
```

### Task 4.5: Video playback research & planning
**Goal**: Investigate video playback capabilities on ESP32-S3

**Hardware constraints identified:**
- Display: 240x320 pixels (portrait) or 320x240 (landscape)
- Color depth: 16-bit RGB565
- LVGL draw buffer: ~1/10 of screen in PSRAM
- Limited processing power for real-time decoding

**Research tasks:**
- Review ESP32-S3 specifications (320x240 max @ 16-bit color)
- Evaluate MJPEG decoder libraries (lightweight, frame-by-frame)
- Test PSRAM bandwidth for video frames (need ~153KB per frame for full screen RGB565)
- Consider ffmpeg pre-processing on PC to reduce resolution/bitrate
- Evaluate frame buffer strategies (single buffer with DMA)

**Create design document:**
- `docs/VIDEO_PLAYBACK_DESIGN.md`
- Document hardware limitations (240x320 display, PSRAM bandwidth)
- Propose video format (MJPEG at 160x120 @ 10fps or 240x180 @ 5fps)
- Outline VideoManager class structure
- Define preprocessing pipeline (ffmpeg conversion)

**Realistic expectations:**
- Full 320x240 video @ 30fps: **NOT FEASIBLE** (too much bandwidth + decoding overhead)
- Recommended: MJPEG at 160x120 @ 10fps (proof-of-concept)
- Better option: MJPEG at 240x180 @ 5-10fps (centered, black bars on sides)
- Consider animated GIF as simpler alternative for short clips

### Task 4.6: Testing
**Testing:**
- Play various audio formats (MP3, WAV)
- Test with corrupted files
- Test with very large files (>10MB)
- Monitor serial output during "Guru Meditation" scenarios
- Test playback stop/start cycles
- Verify memory doesn't leak over multiple plays
- Test SD card removal during playback

---

## Phase 5: System Log Screen Performance

**Goal**: Already optimized - verify and document.

### Task 5.1: Audit current implementation
**Files to review:**
- `src/screens/system_log_screen.cpp` - Lines 150-220

**Current optimizations:**
✅ Uses lv_label instead of lv_textarea (no cursor overhead)
✅ Caps entry count to prevent unbounded growth
✅ Builds display text in PSRAM buffer before setting
✅ Efficient string concatenation
✅ Filters for log level/category

**Assessment**: Already well-optimized per requirements

### Task 5.2: Add performance monitoring
**Files to modify:**
- `src/screens/system_log_screen.cpp`

**Implementation:**
```cpp
void SystemLogScreen::refreshLogDisplay() {
    unsigned long start = micros();

    // ... existing refresh logic ...

    unsigned long duration = micros() - start;
    if (duration > 10000) { // Log if refresh takes >10ms
        Logger::getInstance().warnf("[SystemLog] Refresh took %lu μs", duration);
    }
}
```

### Task 5.3: Add log export functionality
**Files to modify:**
- `src/screens/system_log_screen.cpp` - Add export button

**Implementation:**
```cpp
// Add button to UI
export_btn = lv_btn_create(toolbar);
lv_obj_add_event_cb(export_btn, handleExportButton, LV_EVENT_CLICKED, this);

void SystemLogScreen::handleExportButton(lv_event_t* e) {
    auto* screen = static_cast<SystemLogScreen*>(lv_event_get_user_data(e));

    // Export logs to SD card
    SdCardDriver& sd = SdCardDriver::getInstance();
    File logFile = sd.open("/logs/system_log.txt", "w");

    const auto& entries = Logger::getInstance().getEntries();
    for (const auto& entry : entries) {
        logFile.println(entry.message.c_str());
    }
    logFile.close();

    Logger::getInstance().info("[SystemLog] Logs exported to /logs/system_log.txt");
}
```

### Task 5.4: Testing
**Testing:**
- Generate high-frequency logs (100+ per second)
- Measure refresh performance with 500+ entries
- Test filter changes with large log sets
- Verify scroll performance
- Test log export functionality

---

## Phase 6: Configuration Protection & Developer Tools ✅ COMPLETED

**Goal**: Prevent configuration corruption from power loss during writes, add comprehensive settings enrichment, and provide developer diagnostics.

**Status**: ✅ All tasks completed and tested
**Completion Date**: 2025-11-20
**Build**: Successfully compiled - RAM: 21.1% (69,056 bytes), Flash: 28.3% (1,852,837 bytes)

### Task 6.1: ✅ Implement atomic settings writes
**Files modified:**
- `src/core/storage_manager.cpp` (lines 176-222)
- `src/core/storage_manager.h`

**Implementation completed:**
- ✅ Atomic write pattern using temporary file + rename
- ✅ Write to `/settings.json.tmp` first, then rename to `/settings.json`
- ✅ Prevents corruption on power loss during write
- ✅ Orphaned temp file cleanup on boot

**Code implemented:**
```cpp
bool StorageManager::saveSettings(const SettingsSnapshot& snapshot) {
    const char* temp_file = "/settings.json.tmp";

    // Write to temp file first
    File file = LittleFS.open(temp_file, FILE_WRITE);
    serializeJsonPretty(doc, file);
    file.close();

    // Atomic rename
    if (LittleFS.exists(SETTINGS_FILE)) {
        LittleFS.remove(SETTINGS_FILE);
    }
    LittleFS.rename(temp_file, SETTINGS_FILE);

    Logger::getInstance().debug("[Storage] Settings saved atomically");
    return true;
}
```

### Task 6.2: ✅ Add settings validation on boot
**Files modified:**
- `src/core/storage_manager.cpp` (lines 224-289)

**Implementation completed:**
- ✅ Recovery from interrupted write (checks for orphaned temp file)
- ✅ JSON parse error detection with backup to `/settings.json.corrupted`
- ✅ Settings version validation for migration support
- ✅ Automatic fallback to defaults on corruption

**Code implemented:**
```cpp
bool StorageManager::loadSettings(SettingsSnapshot& snapshot) {
    const char* temp_file = "/settings.json.tmp";

    // Check if temp file exists (interrupted write)
    if (LittleFS.exists(temp_file) && !LittleFS.exists(SETTINGS_FILE)) {
        Logger::getInstance().warn("[Storage] Recovering from interrupted write");
        LittleFS.rename(temp_file, SETTINGS_FILE);
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);

    if (err) {
        // Backup corrupted file for debugging
        LittleFS.rename(SETTINGS_FILE, "/settings.json.corrupted");
        Logger::getInstance().warn("[Storage] Corrupted settings backed up");
        return false;
    }

    // Validate settings version
    uint32_t loaded_version = doc["system"]["settingsVersion"] | 0;
    if (loaded_version == 0) {
        Logger::getInstance().warn("[Storage] Old format - will upgrade");
    }

    return true;
}
```

### Task 6.3: ✅ Add settings backup/restore
**Files modified:**
- `src/core/settings_manager.h` (lines 187-191) - Added backup methods
- `src/core/settings_manager.cpp` (lines 496-616) - Implemented backup/restore

**Implementation completed:**
- ✅ SD card backup to `/config/settings_backup.json`
- ✅ Direct file copy (LittleFS → SD card) for reliability
- ✅ Restore from SD card backup
- ✅ hasBackup() helper method
- ✅ lastBackupTime tracking in SettingsSnapshot
- ✅ Automatic /config directory creation

**Code implemented:**
```cpp
bool SettingsManager::backupToSD() {
    // Create /config directory if needed
    if (!SD_MMC.exists("/config")) {
        SD_MMC.mkdir("/config");
    }

    // Copy LittleFS settings.json to SD card
    File src = LittleFS.open("/settings.json", FILE_READ);
    File backup = SD_MMC.open("/config/settings_backup.json", FILE_WRITE);

    // Copy file contents with 512 byte buffer
    uint8_t buf[512];
    while (src.available()) {
        size_t len = src.read(buf, sizeof(buf));
        backup.write(buf, len);
    }

    src.close();
    backup.close();

    // Update backup timestamp
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lu", millis() / 1000);
    current_.lastBackupTime = timestamp;
    persistSnapshot();

    return true;
}

bool SettingsManager::restoreFromSD() {
    // Copy backup from SD to LittleFS
    File backup = SD_MMC.open("/config/settings_backup.json", FILE_READ);
    File dest = LittleFS.open("/settings.json", FILE_WRITE);

    uint8_t buf[512];
    while (backup.available()) {
        size_t len = backup.read(buf, sizeof(buf));
        dest.write(buf, len);
    }

    backup.close();
    dest.close();

    // Reload settings
    StorageManager::getInstance().loadSettings(current_);
    return true;
}

bool SettingsManager::hasBackup() const {
    return SD_MMC.exists("/config/settings_backup.json");
}
```

### Task 6.4: ✅ Add periodic auto-backup
**Files modified:**
- `src/main.cpp` (lines 349-362)

**Implementation completed:**
- ✅ LVGL timer runs every 30 minutes
- ✅ Automatically backs up settings to SD card
- ✅ Logs success/failure for monitoring
- ✅ Non-blocking operation

**Code implemented:**
```cpp
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
```

### Task 6.5: ✅ Add brown-out detection and boot count tracking
**Files modified:**
- `src/main.cpp` (lines 160-200)
- `src/core/settings_manager.h` (line 49) - Added bootCount field
- `src/core/settings_manager.cpp` (lines 487-494) - incrementBootCount()

**Implementation completed:**
- ✅ Reset reason detection (ESP32-S3 compatible)
- ✅ Boot count incrementation on each startup
- ✅ Logging of reset reason and boot count
- ✅ Brown-out recovery attempt from SD backup
- ✅ Fixed ESP32-S3 reset reason enum values

**Code implemented:**
```cpp
// Get reset reason (ESP32-S3 compatible)
RESET_REASON reset_reason = rtc_get_reset_reason(0);
const char* reset_reason_str = "UNKNOWN";

// Map reset reasons to strings
switch (reset_reason) {
    case POWERON_RESET: reset_reason_str = "POWERON"; break;
    case RTCWDT_RTC_RESET: reset_reason_str = "RTC_WDT"; break;
    case TG0WDT_CPU_RESET: reset_reason_str = "TG0WDT"; break;
    case TG1WDT_CPU_RESET: reset_reason_str = "TG1WDT"; break;
    case RTCWDT_CPU_RESET: reset_reason_str = "RTC_WDT_CPU"; break;
    case RTCWDT_BROWN_OUT_RESET: reset_reason_str = "BROWNOUT"; break;
    // ... etc
}

logger.infof("[System] Reset reason: %s", reset_reason_str);

// Increment and log boot count
settings_mgr.incrementBootCount();
logger.infof("[System] Boot count: %u", settings_mgr.getBootCount());

// Recover from brownout if needed
if (reset_reason == RTCWDT_BROWN_OUT_RESET) {
    logger.warn("[System] Brown-out detected, attempting recovery");
    if (settings_mgr.hasBackup()) {
        settings_mgr.restoreFromSD();
    }
}
```

### Task 6.6: ✅ Enrich SettingsSnapshot with additional fields
**Files modified:**
- `src/core/settings_manager.h` (lines 9-51)
- `src/core/settings_manager.cpp` (lines 301-343) - loadDefaults()
- `src/core/storage_manager.cpp` (lines 12-107) - JSON serialization

**New fields added to SettingsSnapshot:**
- ✅ WiFi & Network: `wifiAutoConnect`, `hostname`
- ✅ BLE: `bleDeviceName`, `bleEnabled`, `bleAdvertising`
- ✅ Display & UI: `screenTimeout`, `autoSleep`, `landscapeLayout`
- ✅ LED: `ledEnabled`
- ✅ Audio: `audioVolume`, `audioEnabled`
- ✅ System: `bootCount`, `settingsVersion`, `lastBackupTime`

**Total settings fields**: ~27 comprehensive settings across all categories

### Task 6.7: ✅ Create Developer Screen for diagnostics
**Files created:**
- `src/screens/developer_screen.h` - Screen class definition
- `src/screens/developer_screen.cpp` - Full implementation with UI and handlers

**Files modified:**
- `src/screens/settings_screen.h` (line 27) - Added handleDeveloperButton
- `src/screens/settings_screen.cpp` (lines 157-166, 335-344) - Developer button
- `src/main.cpp` (line 29, 308, 326) - Registration as hidden app

**Features implemented:**
- ✅ **System Statistics Card**: Version, boot count, hostname, uptime, heap/PSRAM usage, CPU info
- ✅ **Backup Status Card**: SD card status, backup availability, last backup time
- ✅ **Actions Card**:
  - Backup to SD button
  - Restore from SD button
  - Reset All Settings button (red, dangerous action)
- ✅ **Auto-updating stats**: Timer refreshes every 2 seconds
- ✅ **Theme support**: Applies dynamic theme colors
- ✅ **Back button**: Returns to Settings screen
- ✅ **Accessible from Settings**: "Developer" button at bottom of settings

**UI implementation:**
```cpp
// System Statistics display
char stats_text[512];
snprintf(stats_text, sizeof(stats_text),
         "Version: %s\n"
         "Boot Count: %u\n"
         "Hostname: %s\n"
         "Uptime: %02u:%02u:%02u\n\n"
         "Heap: %u KB / %u KB\n"
         "PSRAM: %u KB / %u KB\n"
         "CPU Cores: %d @ %d MHz",
         snapshot.version.c_str(),
         snapshot.bootCount,
         snapshot.hostname.c_str(),
         hours, minutes, seconds,
         free_heap / 1024, total_heap / 1024,
         free_psram / 1024, total_psram / 1024,
         chip_info.cores,
         ESP.getCpuFreqMHz());
```

### Task 6.8: ✅ Testing
**Testing completed:**
- ✅ Device boots successfully with boot count: 3
- ✅ Settings loaded from LittleFS with migration warning (as expected)
- ✅ Developer screen accessible from Settings
- ✅ System statistics display correctly (uptime, memory, CPU info)
- ✅ Backup to SD card works (tested multiple times)
- ✅ Restore from SD card works (tested multiple times)
- ✅ Auto-backup timer started (logs show 30 minute interval)
- ✅ Reset reason logging works (shows "UNKNOWN" for normal boot)
- ✅ Boot count increments on each reboot
- ✅ No compilation errors or warnings
- ✅ Memory usage: RAM 21.1%, Flash 28.3% - excellent headroom

**Serial log verification:**
```
[00000845 ms] [INFO] [System] Boot count: 3
[00001589 ms] [INFO] [Settings] Auto-backup timer started (every 30 minutes)
[00013623 ms] [INFO] [Settings] Launching Developer screen...
[00018193 ms] [INFO] [Settings] Settings restored from backup
[00018193 ms] [INFO] [Developer] Restore from SD card successful
```

### Lessons Learned from Phase 6:

1. **Atomic writes**: Using temp file + rename pattern is more reliable than direct writes
2. **Recovery strategies**: Multiple layers (temp file recovery, corrupted file backup, SD card restore)
3. **ESP32-S3 specifics**: Reset reason enums differ from ESP32, required platform-specific handling
4. **Settings enrichment**: Organized into logical categories (WiFi, BLE, Display, LED, Audio, System)
5. **Developer tools**: Essential for testing and debugging backup/restore functionality
6. **Auto-backup**: Non-blocking LVGL timer approach works well for periodic tasks

---

## Phase 7: Integration Testing & Cleanup

**Goal**: Ensure all features work together cohesively.

### Task 7.1: End-to-end testing scenarios
**Test cases:**

1. **Global Keyboard Flow**
   - Navigate to WiFi settings
   - Focus SSID field → keyboard appears
   - Type text → verify input
   - Switch to BLE settings
   - Focus device name → same keyboard appears
   - Verify no interference between screens

2. **Command Center Integration**
   - Open web interface
   - Navigate to Commands page
   - List all commands
   - Execute BLE keyboard command
   - Verify text appears on connected device
   - Execute audio play command
   - Verify audio plays

3. **File Manager Workflow**
   - Upload audio file via web
   - Create directory via web
   - Navigate to audio player screen
   - Browse uploaded file
   - Play file
   - Check system logs for any errors
   - Download file from web (verify integrity)
   - Delete file via web

4. **Configuration Persistence**
   - Change WiFi settings
   - Change theme settings
   - Trigger backup
   - Reboot device (power cycle)
   - Verify settings restored
   - Simulate corruption (manually corrupt NVS)
   - Verify fallback to backup

5. **Audio Stability**
   - Play 10 different audio files in sequence
   - Monitor memory (should not leak)
   - Play file → stop → play different file (repeat)
   - Test with large files (>5MB)
   - Monitor for Guru Meditation errors
   - Check system logs for decoder issues

### Task 7.2: Memory leak analysis
**Tools:**
- Add heap monitoring to main loop
- Log PSRAM/DRAM deltas every minute
- Run for 24 hours with typical usage patterns

**Files to modify:**
- `src/main.cpp`

```cpp
void loop() {
    static unsigned long last_mem_check = 0;
    static size_t last_psram_free = 0;
    static size_t last_dram_free = 0;

    // ... existing loop ...

    unsigned long now = millis();
    if (now - last_mem_check > 60000) { // Every minute
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);

        int32_t psram_delta = psram_free - last_psram_free;
        int32_t dram_delta = dram_free - last_dram_free;

        if (psram_delta < -10000 || dram_delta < -5000) {
            Logger::getInstance().warnf("[Memory] Possible leak: PSRAM %d, DRAM %d",
                                        psram_delta, dram_delta);
        }

        last_psram_free = psram_free;
        last_dram_free = dram_free;
        last_mem_check = now;
    }
}
```

### Task 7.3: Error recovery testing
**Scenarios:**
- Remove SD card during file operation
- Disconnect WiFi during web upload
- Force audio decoder crash (corrupt file)
- Overflow log buffer
- Corrupt NVS settings
- Brown-out during critical operations

### Task 7.4: Code cleanup
**Tasks:**
- Remove commented-out code
- Fix any TODOs in source files
- Ensure consistent coding style
- Update version number in main.cpp
- Remove debug Serial.print statements (use Logger)

### Task 7.5: Documentation
**Files to create/update:**
- `README.md` - User guide with feature overview
- `docs/API.md` - Web API documentation
- `docs/ARCHITECTURE.md` - System architecture overview
- `docs/TROUBLESHOOTING.md` - Common issues and solutions

---

## Implementation Order Summary

**Recommended sequence:**

1. **Phase 1**: Global Keyboard ✅ **COMPLETED** (0.5 days actual)
   - ✅ High impact, low risk
   - ✅ Improves UX across entire firmware
   - ✅ No external dependencies
   - ✅ Optimized for 240x320 display with 50% height limit
   - **Status**: Fully implemented and compiled successfully

2. **Phase 6**: Configuration Protection & Developer Tools ✅ **COMPLETED** (1 day actual)
   - ✅ Critical for reliability - prevents boot-loop issues
   - ✅ Atomic writes with temp file pattern
   - ✅ SD card backup/restore functionality
   - ✅ Periodic auto-backup every 30 minutes
   - ✅ Brown-out detection and recovery
   - ✅ Boot count tracking
   - ✅ 27 comprehensive settings fields added
   - ✅ Developer screen for diagnostics and testing
   - **Status**: Fully implemented, tested, and deployed

3. **Phase 2**: CommandCenter HTTP Bridge (1-2 days)
   - Moderate complexity
   - Enables remote control
   - Foundation for future LLM integration
   - Builds on existing command infrastructure

4. **Phase 3**: Enhanced File Manager (2-3 days)
   - User-facing feature
   - Builds on existing API
   - Required for convenient audio file management
   - Can reuse KeyboardManager for file renaming

5. **Phase 5**: System Log Export (0.5 days)
   - Already optimized, just needs export feature
   - Low priority but quick win
   - Good to implement before Phase 7 testing

6. **Phase 4**: Audio/Video Improvements (3-5 days)
   - Complex, especially video
   - Split: Audio improvements (1-2 days), then video research (2-3 days)
   - Video may need to be deferred based on 240x320 display constraints
   - Consider animated GIF as simpler alternative

7. **Phase 7**: Integration & Testing (2-3 days)
   - Critical before considering "done"
   - Test all phases together
   - Catches edge cases and memory leaks

**Total estimated time**: 2-3 weeks of focused development
**Time completed**: Phase 1 (0.5 days) + Phase 6 (1 day) = **1.5 days**
**Remaining**: ~8-13 days

---

## Notes & Considerations

### Power Management
- Consider implementing WiFi sleep modes when idle
- BLE advertising can be power-intensive
- Add battery level monitoring if device is portable

### Future Enhancements (Post-Roadmap)
- **LLM/MCP Integration**: Once CommandCenter is HTTP-accessible, add MCP server support
- **OTA Updates**: Implement over-the-air firmware updates via web interface
- **Multi-language Support**: Add language selection in settings
- **Screen Saver**: Blank screen after inactivity to preserve OLED lifespan
- **Touch Calibration**: Add calibration screen for touch accuracy

### Known Limitations
- **Small Display**: 2.8" (240x320) requires careful UI design
  - Standard LVGL widgets may be too large
  - Keyboard needs height limiting (implemented at 50% screen height)
  - Text must be readable at small sizes (Montserrat 14-16pt recommended)
- **Video Playback**: ESP32-S3 will struggle with real-time video decoding at full resolution
  - Display is only 320x240, not suitable for high-quality video
  - Recommended approach: Pre-process videos to MJPEG at 160x120 @ 10fps
  - Alternative: Use animated GIFs for short clips (simpler, better compatibility)
  - External video decoder chip not practical for this size display
- **Audio Format Support**: Limited by ESP32-audioI2S library capabilities
  - Supported: MP3, WAV, AAC, M4A
  - Check library documentation for specific codec support
- **Web UI**: AsyncWebServer doesn't support WebSockets easily
  - Consider for future real-time updates (log streaming, file upload progress)
  - Current implementation uses REST API polling

### Architecture Best Practices
- **Singleton pattern**: Used consistently across managers - good
- **LVGL integration**: All UI through LVGL - maintains consistency
- **Settings persistence**: NVS + JSON - flexible and debuggable
- **Logging system**: Centralized Logger - excellent for debugging
- **Screen lifecycle**: build/onShow/onHide - clean pattern

### Git Workflow Recommendation
After completing each phase:
1. Test thoroughly
2. Commit with descriptive message: `feat: implement global keyboard manager`
3. Tag milestones: `git tag v0.6.0-keyboard`
4. Create branch for experimental features (video playback)

---

## Quick Reference

### Key Files by Feature

**Global Keyboard:**
- `src/core/keyboard_manager.{h,cpp}` (new)
- `src/screens/wifi_settings_screen.{h,cpp}` (modify)
- `src/screens/ble_settings_screen.{h,cpp}` (modify)

**Command Center:**
- `src/core/command_center.{h,cpp}` (enhance)
- `src/core/web_server_manager.{h,cpp}` (enhance)
- `data/www/commands.html` (new)

**File Manager:**
- `src/core/web_server_manager.{h,cpp}` (enhance)
- `data/www/filemanager.html` (new)

**Audio Improvements:**
- `src/core/audio_manager.{h,cpp}` (enhance)
- `src/screens/audio_player_screen.{h,cpp}` (enhance)

**Settings Protection:**
- `src/core/settings_manager.{h,cpp}` (enhance)
- `src/main.cpp` (add backup timer)

### Testing Checklist per Phase

- [ ] Compiles without warnings
- [ ] No memory leaks (run for 10+ minutes)
- [ ] All UI interactions work
- [ ] No crashes/reboots during normal use
- [ ] Logs show no errors
- [ ] Feature works after power cycle
- [ ] Works with SD card inserted/removed
- [ ] Works with WiFi connected/disconnected

---

## Development Best Practices (Lessons from Phase 1)

### UI Design for Small Displays (240x320)
1. **Always test widget sizes**: Default LVGL widgets assume larger displays
2. **Use percentages carefully**: `lv_pct(100)` for width is safe, but height needs limiting
3. **Keyboard sizing**: Limit to 50% screen height to leave room for content
4. **Padding optimization**: Reduce default padding (2px instead of 12px) for compact layouts
5. **Font sizes**: Montserrat 14-16pt is readable, 24pt for headers only

### LVGL Integration Patterns
1. **Event handlers with lambdas**: Clean and concise for simple callbacks
   ```cpp
   lv_obj_add_event_cb(textarea, [](lv_event_t* e) {
       lv_obj_t* ta = lv_event_get_target(e);
       KeyboardManager::getInstance().showForTextArea(ta);
   }, LV_EVENT_FOCUSED, nullptr);
   ```

2. **Singleton managers**: Perfect for global resources like keyboard
   - Initialize once in `main.cpp`
   - Access via `getInstance()` from anywhere
   - No memory leaks, no manual cleanup

3. **Layout updates**: Call `lv_obj_update_layout()` after dynamic positioning
   - Ensures proper rendering after `lv_obj_align()`
   - Prevents visual glitches on first show

### Memory Management
- PSRAM allocation: Use for large buffers (LVGL draw buffer)
- Track memory usage: Log PSRAM/DRAM at key points
- Current usage: RAM 21.0%, Flash 28.1% - plenty of headroom

### Git Workflow
- Commit after each task completion
- Use descriptive messages: `feat: implement KeyboardManager singleton`
- Tag milestones: `git tag v0.6.0-keyboard`
- Test before committing (build + basic functionality)

---

**End of Roadmap** - Phases 1 & 6 completed successfully!

**Next recommended phases:**
- Phase 2: CommandCenter HTTP Bridge (enables remote control)
- Phase 3: Enhanced File Manager (improve web UI)
- Phase 5: System Log Export (quick win)

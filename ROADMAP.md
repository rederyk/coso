# ESP32-S3 Touch Display - Implementation Roadmap

## Project Overview

This roadmap outlines the development plan for the ESP32-S3 Touch Display firmware. It prioritizes features that enhance connectivity, user interaction, and system robustness.

**Completed Milestones:**
- ✅ **Phase 1: Global Keyboard Manager**: A reusable, firmware-wide LVGL keyboard.
- ✅ **Phase 6: Configuration Protection & Developer Tools**: Atomic settings, backup/restore, and a developer diagnostics screen.
- ✅ **BLE HID Implementation**: BLE keyboard, mouse, HID device support (multiple commits: blehiddevice, Mousekeyboard).
- ✅ **BLE UI Integration**: BLE settings screen, remote control screen (blesui commit).
- ✅ **Storage & SD Card**: Storage manager, palette storage, SD card driver and explorer screen (storage, sdcard, storagepalette commits).
- ✅ **LED Enhancements**: RGB LED driver, LED patterns, settings screen, themes (rgbLed, vivetheme, ui LED rework commits).
- ✅ **Theme System**: Theme settings screen, multiple themes (live, night, natural circles, gradients, themes screen (livetheme, nightcircle, livetheme, rgbhcircle commits).
- ✅ **Dock Widgets**: Enhanced dock with scroll, palette, glyph integration (dockscroll, docknatural, glyph commits).
- ✅ **CommandCenter Web Bridge**: HTTP API for executing commands with web UI (webuicommand commit).
- ✅ **LVGL Buffer Optimization**: Runtime buffer switching, DRAM/PSRAM modes (lvglBufferMOde commits).
- ✅ **Font & Emoji Integration**: Custom font generation, emoji support (uicapi, frees up font tools).
- ✅ **WiFi Button Fix & UI Tweaks**: UI improvements, button fixes, card heights (wifibtnfix, ui-card-heightfix, uiFix commits).
- ✅ **System Log Screen**: Log viewer screen with live updates (log commit).
- ✅ **Orientation & Display Handling**: Portrait/landscape support, size optimizations (orient, size commits).
- ✅ **LVGL Double Buffer Mode**: Active DRAM double buffer for optimal performance (DRAM buffer optimizations).
- ✅ **Thread-Safe Architecture**: Core affinity configuration, system tasks for message passing (task_config.h, SystemTasks).
- ✅ **I2C Scanner Utility**: Peripheral diagnostics (i2c_scanner utility).

---

## Phase 2: BLE Enhancements & Command Center

**Goal**: Evolve the BLE capabilities from a simple HID device to a powerful, dual-role system with a remote command interface. This phase will enable control from a companion app (e.g., a mobile phone) and interaction with other BLE devices.

### Task 2.1: Implement a Custom BLE Command Service

**Goal**: Create a custom BLE service to expose the `CommandCenter` over BLE, allowing for remote execution of system commands.

**Implementation:**
1.  **Define BLE Service & Characteristics:**
    -   **Service UUID**: `a1e5a87c-8423-4a21-a4d9-e5a56c29883f`
    -   **Command RX Characteristic (Write)**: `c298e5a5-a4d9-4a21-8423-a1e5a87c883f` (Client writes commands here)
    -   **Response TX Characteristic (Notify)**: `883fa87c-e5a5-4a21-a4d9-c298e5a5c298` (Device sends results back via notifications)

2.  **Define Command Syntax (JSON):**
    -   All communication will be UTF-8 encoded JSON strings.
    -   **Command from Client to ESP32:**
        ```json
        {
          "command": "command_name",
          "args": ["arg1", "arg2", ...],
          "id": "optional_request_id"
        }
        ```
    -   **Response from ESP32 to Client:**
        ```json
        {
          "status": "success" | "error",
          "message": "Descriptive message of result or error",
          "id": "optional_request_id"
        }
        ```

3.  **Integrate with `BleHidManager`:**
    -   In `BleHidManager::init()`, add the custom service and characteristics.
    -   Create a `BLECharacteristicCallbacks` handler for the `CMD_RX_UUID`.
    -   On a write event, the handler will parse the JSON, execute the command via `CommandCenter::getInstance().executeCommand()`, and queue the response.

4.  **Implement Response Mechanism:**
    -   After command execution, serialize the response JSON.
    -   Use the `CMD_TX_UUID` characteristic's `notify()` method to send the response to the connected client.

**Files to modify:**
- `src/core/ble_hid_manager.h` / `src/core/ble_hid_manager.cpp`

---

### Task 2.2: Complete BLE Client Functionality

**Status**: Backend ✅ IMPLEMENTED (BleClientManager exists with scan and connect methods). UI screen pending (BleClientScreen not created yet).

**Goal**: Enable the device to connect to other BLE peripherals, discover their services, and interact with them.

**Implementation:**
1.  **Connection Logic:**
    -   In `BleClientManager`, implement the `connectTo()` method fully. It should handle the connection process and manage the `NimBLEClient` instance. ✅ DONE
2.  **Service Discovery:**
    -   After connecting, add logic to iterate through the peripheral's services and characteristics, logging them for debugging. (Pending full implementation)
3.  **Read/Write/Notify:**
    -   Implement wrapper functions in `BleClientManager` to:
        -   `readCharacteristic(service_uuid, char_uuid)`
        -   `writeCharacteristic(service_uuid, char_uuid, data)`
        -   `subscribeToNotifications(service_uuid, char_uuid, callback)` (Basic connection logic present, wrappers to complete)
4.  **UI Integration:**
    -   Create a new screen, `BleClientScreen`, accessible from `BleSettingsScreen`.
    -   This screen will list discovered devices from a scan.
    -   Tapping a device will attempt to connect.
    -   Once connected, it will display the device's services and characteristics. (Not implemented)

**Files to create/modify:**
- `src/core/ble_client_manager.h` / `src/core/ble_client_manager.cpp` ✅ (Backend exists)
- `src/screens/ble_client_screen.h` / `src/screens/ble_client_screen.cpp` (new) (Pending)
- `src/screens/ble_settings_screen.cpp` (to launch the new screen) (Check if launch access is added)

---

### Task 2.3: Bridge CommandCenter to HTTP (Refinement)

✅ **COMPLETED** (2025-11-22): CommandCenter exposed via HTTP with `/api/commands` endpoint and web UI in `commands.html`.

**Goal**: Expose the `CommandCenter` via a web interface for easy testing and integration. (Refines original Phase 2 tasks).

**Implementation:**
1.  **Create Command Listing Endpoint:**
    -   Implement a `GET /api/commands` endpoint in `WebServerManager`.
    -   This endpoint will return a JSON array of all registered commands, including their names and descriptions.
2.  **Build a Web UI:**
    -   Create `data/www/commands.html`.
    -   The page will fetch the command list from `/api/commands` on load.
    -   It will dynamically generate a simple UI to allow users to enter arguments and execute any command.
    -   The result (`success` or `error`) will be displayed on the page.

**Files to create/modify:**
- `src/core/web_server_manager.cpp`
- `data/www/commands.html` (new)

---

## Phase 3: Web Dashboard & API Gateway

**Goal**: Costruire un server HTTP sicuro e una dashboard web JS leggera prima di attivare il file manager, minimizzando l’impatto su RAM/CPU dell’ESP.

*Riferimento architettura:* `docs/web_dashboard_architecture.md`

### Task 3.1: WebServerManager foundation + hardening
-   Server asincrono sul core di lavoro con handler statici per asset fingerprinted (`data/www/*`, già gzip/brotli).
-   API base `/api/v1/health`, `/api/v1/auth/login|logout`, `/api/v1/logs/tail` (SSE), `/api/v1/commands` (ponte CommandCenter).
-   Sicurezza: API key + session cookie HttpOnly, anti-CSRF via header, CORS off di default, normalizzazione path per bloccare traversal, header CSP/nosniff.
-   Streaming chunked per risposte JSON e log, timeout/keep-alive per non saturare FD.

### Task 3.2: Dashboard SPA shell (JS minimale)
-   Build offline (Vite/Rollup con Preact/Lit o vanilla + htm) con bundle <200KB gzip, caricamento on-demand dei moduli.
-   Schermate: stato WiFi/BLE, heap/PSRAM, log live via SSE, esecuzione comandi (CommandCenter) con form dinamica.
-   Client API centralizzato con retry/backoff, gestione token e fallback offline; UI responsive ma leggera (CSS generato da PostCSS, niente framework pesanti).

### Task 3.3: File Manager API & UI (dopo la shell)
-   Endpoints CRUD SD: `GET /api/v1/fs/list`, `POST /api/v1/fs/upload` (chunked), `POST /api/v1/fs/mkdir`, `POST /api/v1/fs/rename`, `DELETE /api/v1/fs` e download stream.
-   UI file manager come modulo lazy (icone, breadcrumb, drag&drop upload, conferme), usando le API sopra; nessun rendering dal lato ESP.

---

## Phase 4: Audio Player & System Stability

**Goal**: Improve the robustness of the audio player and investigate advanced media capabilities.

*This phase remains as originally defined. Focus is on error handling and stability before exploring new features.*

### Task 4.1: Improve AudioManager Error Handling
-   Introduce detailed error states (`AudioError` enum).
-   Add audio format validation before attempting to play.
-   Implement decoder state monitoring to detect unexpected playback stops.

### Task 4.2: Video Playback Research
-   Investigate the feasibility of MJPEG playback on the ESP32-S3.
-   **Realistic Goal**: Proof-of-concept with a low-resolution, low-framerate video (e.g., 160x120 @ 10fps).
-   Document findings in `docs/VIDEO_PLAYBACK_DESIGN.md`. Full, high-quality video is not a primary goal.

---

## Phase 5: System Log Enhancements

**Goal**: Add export functionality to the already-performant system log screen.

### Task 5.1: Add Log Export to SD Card
-   Add an "Export" button to the `SystemLogScreen`.
-   On click, write the current log buffer to a file on the SD card (e.g., `/logs/system_log_{timestamp}.txt`).

---

## Phase 7: Integration Testing & Cleanup

**Goal**: Ensure all new and existing features work together cohesively and the codebase is clean.

*This phase is critical and remains as originally defined.*

### Task 7.1: End-to-End Testing
-   Test scenarios combining BLE commands, web file uploads, and audio playback.
-   Example: Use the BLE Command Center to trigger a download of a file via HTTP, then play it.

### Task 7.2: Memory Leak Analysis
-   Add heap monitoring to the main loop to track memory usage over a long-running test.

### Task 7.3: Final Documentation
-   Update `README.md` with an overview of the new features.
-   Create `docs/API.md` to document the HTTP and BLE command interfaces.

---

## Future Features & TODO

Based on TODO.md and recent development priorities:

### 🔧 Upcoming Tasks (From TODO.md)
- **LLM MCP Preparation**: Expand CommandCenter instance usage for LLM integrations and MCP (Model Context Protocol) support.
- **Extended Web Interface**: Enhanced web server with file manager for uploading/downloading files from SD card.
- **Audio/Video Player**: SD card-based audio/video player with automatic quality reduction based on screen resolution.
- **Performance Optimization**: Continued improvements following architectural guidelines.

### 🚀 Potential Future Phases

**Phase 8: Media Center**
- Audio playback from SD card with playlist support
- Video playback proof-of-concept (MJPEG low-res)
- File browser for media files

**Phase 9: Voice Assistant Integration**
- Voice input via I2S microphone
- Wake word detection
- Integration with local LLM or cloud services via CommandCenter

**Phase 10: Advanced Networking**
- MQTT client for IoT integration
- WebSocket support in WebServerManager
- Advanced BLE mesh networking

**Phase 11: System Expansion**
- GPIO expansion board support (more peripherals)
- External display/output options
- Modular hardware support

## Deprecated / Merged Phases

-   **Original Phase 2 (Expand CommandCenter)**: The goals of this phase have been merged into the new **Phase 2: BLE Enhancements & Command Center** to provide a more unified approach to remote control.
-   **Original Phase 6 (Configuration Protection)**: ✅ **COMPLETED**.
-   **Original Phase 1 (Global Keyboard)**: ✅ **COMPLETED**.

# ESP32-S3 Touch Display - Implementation Roadmap

## Project Overview

This roadmap outlines the development plan for the ESP32-S3 Touch Display firmware. It prioritizes features that enhance connectivity, user interaction, and system robustness.

**Completed Milestones:**
- ✅ **Phase 1: Global Keyboard Manager**: A reusable, firmware-wide LVGL keyboard.
- ✅ **Phase 6: Configuration Protection & Developer Tools**: Atomic settings, backup/restore, and a developer diagnostics screen.

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

**Goal**: Enable the device to connect to other BLE peripherals, discover their services, and interact with them.

**Implementation:**
1.  **Connection Logic:**
    -   In `BleClientManager`, implement the `connectTo()` method fully. It should handle the connection process and manage the `NimBLEClient` instance.
2.  **Service Discovery:**
    -   After connecting, add logic to iterate through the peripheral's services and characteristics, logging them for debugging.
3.  **Read/Write/Notify:**
    -   Implement wrapper functions in `BleClientManager` to:
        -   `readCharacteristic(service_uuid, char_uuid)`
        -   `writeCharacteristic(service_uuid, char_uuid, data)`
        -   `subscribeToNotifications(service_uuid, char_uuid, callback)`
4.  **UI Integration:**
    -   Create a new screen, `BleClientScreen`, accessible from `BleSettingsScreen`.
    -   This screen will list discovered devices from a scan.
    -   Tapping a device will attempt to connect.
    -   Once connected, it will display the device's services and characteristics.

**Files to create/modify:**
- `src/core/ble_client_manager.h` / `src/core/ble_client_manager.cpp`
- `src/screens/ble_client_screen.h` / `src/screens/ble_client_screen.cpp` (new)
- `src/screens/ble_settings_screen.cpp` (to launch the new screen)

---

### Task 2.3: Bridge CommandCenter to HTTP (Refinement)

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

## Phase 3: Enhanced Web File Manager

**Goal**: Complete the SD card file manager with full CRUD operations and a modern web UI.

*This phase remains as originally defined. The existing API is a good foundation but lacks core features like directory creation and renaming.*

### Task 3.1: Implement Missing API Endpoints
-   **`POST /api/mkdir`**: Create a new directory.
-   **`POST /api/rename`**: Rename a file or directory.

### Task 3.2: Build Modern Web UI
-   Create `data/www/filemanager.html` with a responsive, icon-based interface.
-   Features: Breadcrumb navigation, file/folder icons, upload with drag-and-drop, confirmation dialogs.

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

## Deprecated / Merged Phases

-   **Original Phase 2 (Expand CommandCenter)**: The goals of this phase have been merged into the new **Phase 2: BLE Enhancements & Command Center** to provide a more unified approach to remote control.
-   **Original Phase 6 (Configuration Protection)**: ✅ **COMPLETED**.
-   **Original Phase 1 (Global Keyboard)**: ✅ **COMPLETED**.
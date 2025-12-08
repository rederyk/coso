# Operating Modes Implementation

## Overview

The ESP32-S3 Touch project now supports three operating modes to optimize memory usage based on the primary use case:

1. **UI_ONLY**: UI and Voice Assistant enabled, no Web Server. Ideal for standalone touchscreen operation.
2. **WEB_ONLY**: Web Server enabled, no UI/LVGL. Maximizes memory for web-based interactions.
3. **FULL**: All components enabled (default). Full functionality at the cost of higher memory usage.

These modes are stored in the settings and take effect on boot. Runtime changes require a reboot for full effect, though some services (like web server) adjust immediately.

## Configuration

- **Setting Location**: `operatingMode` in settings.json (via SettingsManager).
- **API Endpoints** (when Web Server is active):
  - GET `/api/system/mode`: Retrieve current mode.
  - POST `/api/system/mode`: Change mode (body: `{"mode": "FULL"}` or `"UI_ONLY"`, `"WEB_ONLY"`).
- **Default**: FULL.

## Memory Optimization

- **WEB_ONLY**: Skips LVGL/UI initialization (saves ~30-50KB DRAM from buffers). Suitable for headless web access.
- **UI_ONLY**: Skips WebServerManager (saves server task and HTTP stack memory, ~20-30KB).
- **FULL**: All features enabled.

Monitor free heap via Serial logs or API `/api/health`.

## Voice Assistant Integration

- Voice mode handling respects the current operating mode: no UI suspension in WEB_ONLY.
- Web-initiated voice recording initializes voice assistant if needed, with conditional LVGL suspension.

## Usage Notes

- Change modes via web interface (Settings > Advanced > Operating Mode) or API.
- Reboot after changes for complete initialization.
- In WEB_ONLY, access via http://[IP]:80.
- In UI_ONLY, use touchscreen UI exclusively.

## Future Enhancements

- Runtime mode switching without reboot (dynamic UI load/unload).
- Mode-specific power profiles.

Generated on 2025-08-12.

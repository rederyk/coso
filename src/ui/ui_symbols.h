#ifndef UI_SYMBOLS_H
#define UI_SYMBOLS_H

#include <lvgl.h>

/**
 * @file ui_symbols.h
 * @brief UI Symbol definitions using LVGL built-in FontAwesome icons
 *
 * This header provides semantic aliases for LVGL symbols to maintain
 * code readability while using zero-overhead built-in icons instead of
 * custom emoji fonts.
 *
 * All symbols are from FontAwesome and included in LVGL's default fonts.
 * No additional Flash/RAM overhead.
 */

// App/Screen icons
#define UI_SYMBOL_HOME          LV_SYMBOL_HOME         // 🏠 Home screen
#define UI_SYMBOL_SETTINGS      LV_SYMBOL_SETTINGS     // ⚙️ Settings
#define UI_SYMBOL_SYSLOG        LV_SYMBOL_LIST         // 🛰️ System log (alternative: LV_SYMBOL_BARS)
#define UI_SYMBOL_INFO          LV_SYMBOL_FILE         // ℹ️ Info (alternative: LV_SYMBOL_LIST)

// Status/Feedback icons
#define UI_SYMBOL_OK            LV_SYMBOL_OK           // ✅ Success/OK
#define UI_SYMBOL_ERROR         LV_SYMBOL_CLOSE        // ❌ Error
#define UI_SYMBOL_WARNING       LV_SYMBOL_WARNING      // ⚠️ Warning
#define UI_SYMBOL_REFRESH       LV_SYMBOL_REFRESH      // 🔄 Refresh/Reload

// Feature icons
#define UI_SYMBOL_THEME         LV_SYMBOL_TINT         // 🎨 Theme/Color picker
#define UI_SYMBOL_WIFI          LV_SYMBOL_WIFI         // 📶 WiFi
#define UI_SYMBOL_BRIGHTNESS    LV_SYMBOL_CHARGE       // 💡 Display brightness (lightning bolt)
#define UI_SYMBOL_LED           LV_SYMBOL_CHARGE       // 💡 RGB LED
#define UI_SYMBOL_TRASH         LV_SYMBOL_TRASH        // 🗑️ Delete/Trash

// System info icons
#define UI_SYMBOL_CHIP          LV_SYMBOL_SD_CARD      // 🖥️ Chip/Hardware (alternative: LV_SYMBOL_KEYBOARD)
#define UI_SYMBOL_POWER         LV_SYMBOL_CHARGE       // ⚡ Power/Performance
#define UI_SYMBOL_CHART         LV_SYMBOL_BARS         // 📊 Charts/Statistics
#define UI_SYMBOL_STORAGE       LV_SYMBOL_SD_CARD      // 💾 Flash storage
#define UI_SYMBOL_TOOL          LV_SYMBOL_SETTINGS     // 🔧 Tools/SDK

// Additional useful symbols
#define UI_SYMBOL_BATTERY_FULL  LV_SYMBOL_BATTERY_FULL // 🔋 Battery full
#define UI_SYMBOL_GPS           LV_SYMBOL_GPS          // 📡 GPS/Satellite
#define UI_SYMBOL_BLUETOOTH     LV_SYMBOL_BLUETOOTH    // Bluetooth
#define UI_SYMBOL_DIRECTORY     LV_SYMBOL_DIRECTORY    // 📁 Directory/Folder
#define UI_SYMBOL_SAVE          LV_SYMBOL_SAVE         // 💾 Save
#define UI_SYMBOL_EDIT          LV_SYMBOL_EDIT         // ✏️ Edit
#define UI_SYMBOL_BELL          LV_SYMBOL_BELL         // 🔔 Notifications
#define UI_SYMBOL_IMAGE         LV_SYMBOL_IMAGE        // 🖼️ Image/Photo

#endif // UI_SYMBOLS_H

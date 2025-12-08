#ifndef OPERATING_MODES_H
#define OPERATING_MODES_H

/**
 * @file operating_modes.h
 * @brief Defines the operating modes for the device to optimize memory usage.
 */

typedef enum {
    OPERATING_MODE_UI_ONLY,      ///< UI and Voice Assistant enabled, no Web Server
    OPERATING_MODE_WEB_ONLY,     ///< Web Server enabled, no UI/LVGL
    OPERATING_MODE_FULL          ///< All components enabled (default)
} OperatingMode_t;

#endif // OPERATING_MODES_H

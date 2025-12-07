#include "touch_driver.h"
#include "core/display_manager.h"
#include "lvgl_power_manager.h"
#include <Wire.h>
#include <Arduino.h>
#include <algorithm>
#include "utils/logger.h"

#define FT6336_ADDR 0x38
#define FT6336_REG_NUM_TOUCHES 0x02
#define FT6336_REG_TOUCH1_XH 0x03
#define FT6336_REG_TOUCH1_XL 0x04
#define FT6336_REG_TOUCH1_YH 0x05
#define FT6336_REG_TOUCH1_YL 0x06

static bool touch_detected = false;
static bool touch_available = false;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static lv_point_t last_point = {0, 0};

static uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) {
        return 0;
    }

    Wire.requestFrom(FT6336_ADDR, 1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

static void transformToDisplay(uint16_t raw_x, uint16_t raw_y, lv_point_t& point) {
    uint16_t x = raw_x;
    uint16_t y = raw_y;
    uint16_t max_x = TOUCH_MAX_RAW_X;
    uint16_t max_y = TOUCH_MAX_RAW_Y;

#if TOUCH_SWAP_XY
    uint16_t tmp = x;
    x = y;
    y = tmp;
    tmp = max_x;
    max_x = max_y;
    max_y = tmp;
#endif

#if TOUCH_INVERT_X
    if (max_x > 0) {
        x = (x <= max_x) ? (max_x - x) : 0;
    }
#endif

#if TOUCH_INVERT_Y
    if (max_y > 0) {
        y = (y <= max_y) ? (max_y - y) : 0;
    }
#endif

    uint16_t denom_x = (max_x > 1) ? static_cast<uint16_t>(max_x - 1) : 1;
    uint16_t denom_y = (max_y > 1) ? static_cast<uint16_t>(max_y - 1) : 1;

    DisplayManager& display = DisplayManager::getInstance();
    bool landscape = display.isLandscape();

    const lv_coord_t base_width = std::max<lv_coord_t>(1, landscape ? display.getWidth() : display.getHeight());
    const lv_coord_t base_height = std::max<lv_coord_t>(1, landscape ? display.getHeight() : display.getWidth());

    uint32_t scaled_x = ((uint32_t)x * (base_width - 1)) / denom_x;
    uint32_t scaled_y = ((uint32_t)y * (base_height - 1)) / denom_y;

    if (scaled_x >= static_cast<uint32_t>(base_width)) scaled_x = base_width - 1;
    if (scaled_y >= static_cast<uint32_t>(base_height)) scaled_y = base_height - 1;

    lv_coord_t base_x = static_cast<lv_coord_t>(scaled_x);
    lv_coord_t base_y = static_cast<lv_coord_t>(scaled_y);

    if (landscape) {
        point.x = base_x;
        point.y = base_y;
    } else {
        const lv_coord_t portrait_width = std::max<lv_coord_t>(1, display.getWidth());
        const lv_coord_t portrait_height = std::max<lv_coord_t>(1, display.getHeight());
        lv_coord_t rotated_x = portrait_width - 1 - base_y;
        lv_coord_t rotated_y = base_x;

        if (rotated_x < 0) rotated_x = 0;
        if (rotated_x >= portrait_width) rotated_x = portrait_width - 1;
        if (rotated_y < 0) rotated_y = 0;
        if (rotated_y >= portrait_height) rotated_y = portrait_height - 1;

        point.x = rotated_x;
        point.y = rotated_y;
    }
}

void touch_driver_init() {
    auto& logger = Logger::getInstance();
    logger.info("\n[Touch] === Touch Controller Initialization ===");

    touch_available = false;

#if TOUCH_RST >= 0
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, HIGH);
#endif
#if TOUCH_INT >= 0
    pinMode(TOUCH_INT, INPUT_PULLUP);
#endif

    // Reset hardware se disponibile
#if TOUCH_RST >= 0
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);
#endif

    // Prova prima con clock più basso per compatibilità
    Wire.begin(TOUCH_I2C_SDA, TOUCH_I2C_SCL);
    Wire.setClock(100000); // 100kHz per maggiore compatibilità
    delay(200);

    logger.infof("[Touch] I2C pins: SDA=%d, SCL=%d", TOUCH_I2C_SDA, TOUCH_I2C_SCL);
    logger.info("[Touch] Attempting communication with FT6336 at 0x38...");

    // Test connessione base
    Wire.beginTransmission(FT6336_ADDR);
    uint8_t error = Wire.endTransmission();
    const char* error_desc = "(Other error)";
    if (error == 0) error_desc = "(OK)";
    else if (error == 2) error_desc = "(NACK on address)";
    else if (error == 3) error_desc = "(NACK on data)";
    logger.infof("[Touch] Transmission result: %d %s", error, error_desc);

    if (error == 0) {
        // Prova a leggere registri
        delay(50);
        uint8_t vendorID = readRegister(0xA8);
        uint8_t chipID = readRegister(0xA3);
        uint8_t fwVer = readRegister(0xA6);

        logger.infof("[Touch] Vendor ID: 0x%02X", vendorID);
        logger.infof("[Touch] Chip ID: 0x%02X", chipID);
        logger.infof("[Touch] FW Version: 0x%02X", fwVer);
        logger.info("[Touch] ✓ FT6336 detected and ready!");
        touch_available = true;
    } else {
        logger.warn("[Touch] ✗ Touch controller NOT responding!");
        logger.warn("[Touch] This board may not have touch capability,");
        logger.warn("[Touch] or touch may use different pins/protocol.");
        logger.warn("[Touch] Touch input will remain registered for debugging.");
    }

    logger.info("[Touch] ======================================\n");
}

bool touch_driver_available() {
    return touch_available;
}

void touch_driver_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    uint8_t num_touches = readRegister(FT6336_REG_NUM_TOUCHES);

    if (num_touches > 0) {
        // Notify power manager of touch activity
        LVGLPowerMgr.onTouchDetected();

        // Leggi coordinate
        uint8_t xh = readRegister(FT6336_REG_TOUCH1_XH);
        uint8_t xl = readRegister(FT6336_REG_TOUCH1_XL);
        uint8_t yh = readRegister(FT6336_REG_TOUCH1_YH);
        uint8_t yl = readRegister(FT6336_REG_TOUCH1_YL);

        last_x = ((xh & 0x0F) << 8) | xl;
        last_y = ((yh & 0x0F) << 8) | yl;

        lv_point_t point;
        transformToDisplay(last_x, last_y, point);

        data->point = point;
        data->state = LV_INDEV_STATE_PRESSED;
        last_point = point;

        touch_detected = true;
    } else {
        data->point = last_point;
        data->state = LV_INDEV_STATE_RELEASED;
        touch_detected = false;
    }
}

bool touch_driver_has_touch() {
    if (!touch_available) {
        return false;
    }
    return readRegister(FT6336_REG_NUM_TOUCHES) > 0;
}

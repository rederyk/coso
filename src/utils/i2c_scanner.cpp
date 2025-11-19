#include "i2c_scanner.h"
#include <Wire.h>
#include "utils/logger.h"

void scanI2CBus(int sda, int scl) {
    auto& logger = Logger::getInstance();
    logger.infof("\n[I2C Scanner] Testing SDA=%d, SCL=%d", sda, scl);

    Wire.begin(sda, scl);
    Wire.setClock(100000); // 100kHz per sicurezza
    delay(100);

    int devicesFound = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            char buffer[48];
            snprintf(buffer, sizeof(buffer), "  ✓ Device found at 0x%02X", addr);
            String line(buffer);
            if (addr == 0x38) line += " (FT6336/FT6236)";
            else if (addr == 0x15) line += " (GT911)";
            else if (addr == 0x5D) line += " (GT911 alt)";
            else if (addr == 0x14) line += " (FT5x06)";
            logger.info(line.c_str());
            devicesFound++;
        }
    }

    if (devicesFound == 0) {
        logger.warn("  ✗ No I2C devices found");
    }

    Wire.end();
}

void findTouchController() {
    auto& logger = Logger::getInstance();
    logger.info("\n========================================");
    logger.info("     I2C Touch Controller Scanner");
    logger.info("========================================");

    // Pin combinations comuni per ESP32-S3
    const int pinPairs[][2] = {
        {8, 9},    // Comune per ESP32-S3
        {6, 7},    // Alternativa
        {17, 18},  // Già provato
        {1, 2},    // Touch pins T1/T2
        {4, 5},    // GPIO generici
        {14, 21},  // Alternativa
    };

    for (int i = 0; i < 6; i++) {
        scanI2CBus(pinPairs[i][0], pinPairs[i][1]);
        delay(100);
    }

    logger.info("========================================");
    logger.info("Scan complete!");
    logger.info("========================================\n");
}

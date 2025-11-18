#include "i2c_scanner.h"
#include <Wire.h>

void scanI2CBus(int sda, int scl) {
    Serial.printf("\n[I2C Scanner] Testing SDA=%d, SCL=%d\n", sda, scl);

    Wire.begin(sda, scl);
    Wire.setClock(100000); // 100kHz per sicurezza
    delay(100);

    int devicesFound = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Serial.printf("  ✓ Device found at 0x%02X", addr);

            // Identifica dispositivi comuni
            if (addr == 0x38) Serial.print(" (FT6336/FT6236)");
            else if (addr == 0x15) Serial.print(" (GT911)");
            else if (addr == 0x5D) Serial.print(" (GT911 alt)");
            else if (addr == 0x14) Serial.print(" (FT5x06)");

            Serial.println();
            devicesFound++;
        }
    }

    if (devicesFound == 0) {
        Serial.println("  ✗ No I2C devices found");
    }

    Wire.end();
}

void findTouchController() {
    Serial.println("\n========================================");
    Serial.println("     I2C Touch Controller Scanner");
    Serial.println("========================================");

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

    Serial.println("========================================");
    Serial.println("Scan complete!");
    Serial.println("========================================\n");
}

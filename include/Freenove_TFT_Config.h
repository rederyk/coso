#pragma once

// Custom TFT_eSPI setup derived from
// DOC/Freenove_ESP32_S3_Display-main/Libraries/TFT_eSPI_Setups_v1.2/TFT_eSPI_Setups/FNK0104B_2.8_240x320_ILI9341.h

#ifndef USER_SETUP_LOADED
#define USER_SETUP_LOADED
#endif

#ifndef USER_SETUP_INFO
#define USER_SETUP_INFO "Freenove ESP32-S3 Display (2.8in ILI9341)"
#endif
#define TOUCH_CS -1
#define ILI9341_2_DRIVER
#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_ON
#define TFT_WIDTH 240
#define TFT_HEIGHT 320

#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS 10
#define TFT_DC 46
#define TFT_RST -1
#define TFT_BL 45
#define TFT_BACKLIGHT_ON HIGH
#define USE_HSPI_PORT

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY 40000000

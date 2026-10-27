#pragma once

#include <Arduino.h>

// ==========================================
// Credentials Inclusion
// ==========================================
#if __has_include("credentials.h")
  #include "credentials.h"
#elif __has_include("../credentials.h")
  #include "../credentials.h"
#elif __has_include("src/credentials.h")
  #include "src/credentials.h"
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

// ==========================================
// Hardware Pin Definitions
// ==========================================

// ST7305 SPI Display (400x300 Reflective LCD)
#define PIN_LCD_MOSI       12
#define PIN_LCD_CLK        11
#define PIN_LCD_CS         40
#define PIN_LCD_DC         5
#define PIN_LCD_RST        41

// Display dimensions
#define LCD_WIDTH          400
#define LCD_HEIGHT         300

// SDMMC 1-Bit Mode
#define PIN_SDMMC_CLK      38
#define PIN_SDMMC_CMD      21
#define PIN_SDMMC_D0       39
#define SDMMC_MOUNT_POINT  "/sdcard"
#define TASKS_DIR          "/sdcard/tasks"
#define TASKS_FILE_PATH    "/sdcard/tasks/tasks.json"
#define TASKS_TMP_PATH     "/sdcard/tasks/tasks.tmp"

// I2C Bus (SHTC3 & RTC)
#define PIN_I2C_SDA        13
#define PIN_I2C_SCL        14
#define SHTC3_I2C_ADDR     0x70

// Buttons (Active LOW with internal pull-up)
#define PIN_BUTTON_KEY     18
#define PIN_BUTTON_BOOT    0

// Battery Monitoring ADC (ADC1 Channel 3)
#define PIN_BATTERY_ADC    4

// ES8311 I2C & Audio Power Amplifier
#define ES8311_I2C_ADDR    0x18
#define PIN_AUDIO_PA       46

// I2S Bus (ES8311 Audio Codec)
#define PIN_I2S_MCLK       16
#define PIN_I2S_BCLK       9
#define PIN_I2S_WS         45
#define PIN_I2S_DOUT       8

// ==========================================
// Network & Server Settings
// ==========================================
#define MDNS_HOSTNAME      "screen"
#define HTTP_SERVER_PORT   80
#define NTP_SERVER         "pool.ntp.org"
#define NTP_TIMEZONE       "CET-1CEST,M3.5.0,M10.5.0/3"

// ==========================================
// UI / Gesture Timing Settings
// ==========================================
#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 600
#define VISIBLE_TASK_ROWS  6

// ==========================================
// Celebration Feedback Settings
// ==========================================
#define CELEBRATION_DURATION_MS 3000
#define CELEBRATION_VOLUME      70
#define CELEBRATION_IMAGE_DIR   "/images"
#define CELEBRATION_IMAGE_PATH  "/images/celebrate.bmp"



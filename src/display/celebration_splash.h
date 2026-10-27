#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

class CelebrationSplash {
public:
  // Draws the celebration splash image:
  // 1. Attempts to load and render /sdcard/celebrate.bmp
  // 2. Falls back to embedded PROGMEM celebration artwork if SD card or file is unavailable
  static void draw(U8G2 *u8g2);

private:
  static bool drawBmpData(U8G2 *u8g2, const uint8_t *data, size_t len);
  static bool drawFromSd(U8G2 *u8g2, const char *path);
  static void drawFallback(U8G2 *u8g2);
};

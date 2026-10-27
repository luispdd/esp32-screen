#include "celebration_splash.h"
#include "../config.h"
#include <SD_MMC.h>
#include <esp_log.h>

static const char *TAG = "SPLASH";

// Default celebration BMP asset embedded directly from src/assets/default_celebrate.bmp
extern "C" const uint8_t default_celebrate_bmp_start[];
extern "C" const uint8_t default_celebrate_bmp_end[];

#pragma pack(push, 1)
struct BmpHeader {
  uint16_t type;            // 0x4D42 ("BM")
  uint32_t size;
  uint16_t reserved1;
  uint16_t reserved2;
  uint32_t offset;          // Offset to pixel array
  uint32_t dib_header_size; // 40 (BITMAPINFOHEADER)
  int32_t  width;           // 400
  int32_t  height;          // 300 (bottom-to-top) or -300 (top-to-bottom)
  uint16_t planes;          // 1
  uint16_t bpp;             // 1
  uint32_t compression;    // 0 = BI_RGB
  uint32_t img_size;
  int32_t  x_res;
  int32_t  y_res;
  uint32_t num_colors;
  uint32_t important_colors;
};
#pragma pack(pop)

bool CelebrationSplash::drawBmpData(U8G2 *u8g2, const uint8_t *data, size_t len) {
  if (u8g2 == nullptr || data == nullptr || len < sizeof(BmpHeader)) {
    return false;
  }

  const BmpHeader *header = reinterpret_cast<const BmpHeader *>(data);

  // Validate standard 400x300 1-bit monochrome uncompressed BMP
  if (header->type != 0x4D42 || header->bpp != 1 || header->compression != 0 ||
      header->width != LCD_WIDTH || abs(header->height) != LCD_HEIGHT) {
    Serial.printf("[SPLASH] Unsupported BMP: %dx%d, %d bpp, comp=%d\n",
                  (int)header->width, (int)abs(header->height), (int)header->bpp, (int)header->compression);
    return false;
  }

  // Palette is located immediately after DIB header (14 + dib_header_size)
  size_t paletteOffset = 14 + header->dib_header_size;
  if (paletteOffset + 8 > len) {
    return false;
  }

  const uint8_t *palette = data + paletteOffset;
  uint32_t lum0 = (uint32_t)palette[0] + palette[1] + palette[2];
  uint32_t lum1 = (uint32_t)palette[4] + palette[5] + palette[6];
  uint8_t darkBit = (lum0 < lum1) ? 0 : 1;

  // In BMP, row size is padded to a multiple of 4 bytes:
  // For width 400: (400 + 31) / 32 * 4 = 52 bytes per row.
  const size_t rowBytes = ((LCD_WIDTH + 31) / 32) * 4;
  if (header->offset + (rowBytes * LCD_HEIGHT) > len) {
    Serial.println("[SPLASH] BMP data truncated");
    return false;
  }

  bool bottomUp = (header->height > 0);
  const uint8_t *pixels = data + header->offset;

  for (int y = 0; y < LCD_HEIGHT; y++) {
    int screenY = bottomUp ? (LCD_HEIGHT - 1 - y) : y;
    const uint8_t *row = pixels + (y * rowBytes);

    for (int x = 0; x < LCD_WIDTH; x++) {
      uint8_t byteVal = row[x >> 3];
      uint8_t bit = (byteVal >> (7 - (x & 7))) & 0x01;
      if (bit == darkBit) {
        u8g2->drawPixel(x, screenY);
      }
    }
  }

  return true;
}

bool CelebrationSplash::drawFromSd(U8G2 *u8g2, const char *path) {
  if (!SD_MMC.exists(path)) {
    return false;
  }

  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    Serial.printf("[SPLASH] Failed to open %s\n", path);
    return false;
  }

  size_t size = f.size();
  if (size < sizeof(BmpHeader) || size > 32768) {
    Serial.printf("[SPLASH] Invalid file size for BMP: %u bytes\n", (unsigned)size);
    f.close();
    return false;
  }

  uint8_t *buffer = (uint8_t *)malloc(size);
  if (!buffer) {
    Serial.println("[SPLASH] Failed to allocate memory for BMP read");
    f.close();
    return false;
  }

  size_t readBytes = f.read(buffer, size);
  f.close();

  if (readBytes != size) {
    Serial.println("[SPLASH] Incomplete read from SD card");
    free(buffer);
    return false;
  }

  bool ok = drawBmpData(u8g2, buffer, size);
  free(buffer);

  if (ok) {
    Serial.printf("[SPLASH] Successfully rendered celebration image from SD: %s\n", path);
  } else {
    Serial.printf("[SPLASH] Failed to render BMP from %s\n", path);
  }
  return ok;
}

void CelebrationSplash::drawFallback(U8G2 *u8g2) {
  // Pure image asset stored in codebase: NO text, NO generated code
  size_t len = default_celebrate_bmp_end - default_celebrate_bmp_start;
  if (!drawBmpData(u8g2, default_celebrate_bmp_start, len)) {
    Serial.println("[SPLASH] Failed to parse default celebration BMP from codebase");
  } else {
    Serial.println("[SPLASH] Rendered default celebration BMP from codebase");
  }
}

void CelebrationSplash::draw(U8G2 *u8g2) {
  if (u8g2 == nullptr) return;

  u8g2->clearBuffer();

  bool sdSuccess = false;

  // 1. Check if default celebration image file exists in "images/" folder on SD card
  if (SD_MMC.exists(CELEBRATION_IMAGE_PATH)) {
    sdSuccess = drawFromSd(u8g2, CELEBRATION_IMAGE_PATH);
  } else if (SD_MMC.exists(CELEBRATION_IMAGE_DIR)) {
    // 2. Scan "images/" folder for any .bmp file
    File dir = SD_MMC.open(CELEBRATION_IMAGE_DIR);
    if (dir && dir.isDirectory()) {
      File entry = dir.openNextFile();
      while (entry && !sdSuccess) {
        String name = entry.name();
        if (!entry.isDirectory() && (name.endsWith(".bmp") || name.endsWith(".BMP"))) {
          String fullPath = String(CELEBRATION_IMAGE_DIR) + "/" + name;
          sdSuccess = drawFromSd(u8g2, fullPath.c_str());
        }
        entry = dir.openNextFile();
      }
      dir.close();
    }
  }

  // 3. If no image found in "images/" on SD card, render the default image stored in codebase
  if (!sdSuccess) {
    drawFallback(u8g2);
  }
}

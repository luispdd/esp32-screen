---
name: esp32-s3-rlcd
description: Operational knowledge, hardware quirks, flashing rules, and driver setup for the Waveshare ESP32-S3-RLCD-4.2 board.
---

# Waveshare ESP32-S3-RLCD-4.2 Hardware & Firmware Reference

## Tooling & Partition Table
- **Partition Table**: Default 16MB table causes bootloader flash size mismatch. Use a single factory app partition `0x700000` (7 MB) at `0x10000` in `default_16MB.csv`.
- **Upload Command**: `pio run -t upload --upload-port /dev/ttyACM0` (ESP32-S3 USB CDC).
- **Serial Monitoring**: Serial is on USB-CDC (`/dev/ttyACM0` at 115200 baud).
- **Raw Asset Embedding (.incbin)**: GCC dependency tracking (`-MMD`) ignores assembly `.incbin`. Assets in `src/assets/` require a pre-build script (`scripts/check_assets.py`) via `extra_scripts` in `platformio.ini` to touch the assembly file and invalidate cache on asset edits.

## Hardware & Driver Quirks
- **ST7305 RLCD Polarity**: Command `0x20` configures normal non-inverted display mode (white/reflective background with dark text). Command `0x21` inverts the display (black background).
- **SDMMC 1-Bit Mode & Paths**: MicroSD uses hardware SDMMC 1-bit pins (`CLK: 38`, `CMD: 21`, `D0: 39`). Standard SPI SD (`SD.h`) will not work. In `SD_MMC.open()` / `exists()`, use paths relative to card root (e.g. `"/images"`); the `/sdcard` mountpoint is prepended internally. POSIX calls (`fopen`, `stat`) require the full mountpoint `/sdcard/...`.
- **Audio Codec (ES8311) & PA**:
  - Pins: I2C `0x18` on shared `Wire` (`SDA: 13, SCL: 14`), I2S (`MCLK: 16`, `BCLK: 9`, `WS: 45`, `DOUT: 8`), Power Amp enable `GPIO 46` (active HIGH).
  - Clock Ordering: Enable I2S channel (`i2s_channel_enable`) *before* I2C config to provide MCLK on GPIO 16; set reg `0x00` to `0x80` (CSM ON, slave mode).
  - Volume Scaling: ES8311 reg `0x32` unity gain (0dB) is `0xBF`; `0xFF` is +32dB boost. Scale volume to max `0xBF` to prevent speaker overdrive.
  - PA Gating: Drive `GPIO 46` LOW when idle to eliminate speaker hiss; assert HIGH only during active playback.
- **Wi-Fi Power Management**: Call `WiFi.setSleep(false)` on init; 802.11 sleep causes packet drops and connection timeouts on Wi-Fi 6 access points.
- **Header Path for Credentials**: `build_flags = -Isrc` in `platformio.ini` is required so `#if __has_include("credentials.h")` succeeds in nested directories (`src/network/`, etc.).

## REST API Operational Contract
- **Payload Format**: `POST http://screen.local/tasks` strictly expects a JSON array `[{"text": "..."}]`. Sending a raw object `{"text": "..."}` returns HTTP 400.


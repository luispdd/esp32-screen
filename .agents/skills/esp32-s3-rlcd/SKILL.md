---
name: esp32-s3-rlcd
description: Operational knowledge, hardware quirks, flashing rules, and driver setup for the Waveshare ESP32-S3-RLCD-4.2 board.
---

# Waveshare ESP32-S3-RLCD-4.2 Hardware & Firmware Reference

## Tooling & Partition Table
- **Partition Table**: Default 16MB table causes bootloader flash size mismatch. Use a single factory app partition `0x700000` (7 MB) at `0x10000` in `default_16MB.csv`.
- **Upload Command**: `pio run -t upload --upload-port /dev/ttyACM0` (ESP32-S3 USB CDC).
- **Serial Monitoring**: Serial is on USB-CDC (`/dev/ttyACM0` at 115200 baud).

## Hardware & Driver Quirks
- **ST7305 RLCD Polarity**: Command `0x20` configures normal non-inverted display mode (white/reflective background with dark text). Command `0x21` inverts the display (black background).
- **SDMMC 1-Bit Mode**: MicroSD uses hardware SDMMC 1-bit pins (`CLK: 38`, `CMD: 21`, `D0: 39`). Standard SPI SD (`SD.h`) will not work on these lines.
- **Wi-Fi Power Management**: Call `WiFi.setSleep(false)` on init; 802.11 sleep causes packet drops and connection timeouts on Wi-Fi 6 access points.
- **Header Path for Credentials**: `build_flags = -Isrc` in `platformio.ini` is required so `#if __has_include("credentials.h")` succeeds in nested directories (`src/network/`, etc.).

## REST API Operational Contract
- **Payload Format**: `POST http://screen.local/tasks` strictly expects a JSON array `[{"text": "..."}]`. Sending a raw object `{"text": "..."}` returns HTTP 400.

# Operational Guidelines for ESP32-Screen

## Core Commands
- **Build Firmware**: `pio run`
- **Flash Firmware**: `pio run -t upload --upload-port /dev/ttyACM0`
- **OpenSpec Validation**: `openspec validate --changes` (do not pass `--change <name>`)

## Operational Gotchas
- **Hardware & Flashing Details**: See skill `esp32-s3-rlcd` for board partitions, ST7305 RLCD polarity, and SDMMC 1-bit pins.
- **REST API Payload**: `POST http://screen.local/tasks` strictly requires a JSON array (e.g. `[{"text": "foo"}]`). Raw JSON objects return HTTP 400. Complete OpenAPI contract in `openapi.yaml` / `openapi.json`.
- **Wi-Fi Secrets**: Place Wi-Fi credentials in `src/credentials.h` (untracked); never commit actual credentials.

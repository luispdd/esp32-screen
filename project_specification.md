# Project Specification: ESP32-S3 RLCD To-Do Screen & Local Server

## 1. Executive Summary
Build a standalone, battery-friendly desk To-Do list appliance using the **Waveshare ESP32-S3-RLCD-4.2** development board. The device connects to local Wi-Fi, synchronizes time via NTP, hosts a lightweight HTTP REST server accessible via mDNS (`screen.local`), displays a scrollable task list on its 4.2" reflective LCD (ST7305), allows physical button navigation to toggle task completion, and persists all task data to an onboard MicroSD card.

---

## 2. Hardware Target & Environment
* **MCU:** ESP32-S3 (16MB Flash, 8MB Octal OPI PSRAM).
* **Framework:** Arduino C/C++ (ESP32 Arduino Core >= 3.0 or PlatformIO equivalent).
* **Display:** 4.2-inch Monochrome Reflective LCD (400×300, ST7305 driver via SPI).
* **Storage:** Onboard MicroSD card slot via `SD_MMC`.
* **Controls:**
  * `KEY` (GPIO 18, Active LOW, internal pull-up)
  * `BOOT` (GPIO 0, Active LOW, internal pull-up)
* **Status LED / USB CDC:** USB CDC on Boot enabled for serial logging over USB-C.

### Pinout Mapping
* **ST7305 SPI Display:** `MOSI: 12`, `CLK: 11`, `CS: 40`, `DC: 5`, `RST: 41`
* **I2C Bus (RTC & SHTC3):** `SDA: 13`, `SCL: 14`
* **MicroSD:** `SD_MMC` peripheral interface (1-bit or 4-bit bus mode).

---

## 3. Core Functional Requirements

### 3.1 Network & Time Synchronization
* Hardcoded Wi-Fi credentials in `config.h` (`WIFI_SSID`, `WIFI_PASSWORD`).
* Connects in Station mode (`WIFI_STA`) on boot with non-blocking retry logic.
* Registers on local network using mDNS responder with hostname: `screen.local`.
* Syncs local date and time on boot via NTP (`pool.ntp.org`) with CET/CEST timezone handling (`CET-1CEST,M3.5.0,M10.5.0/3`).

### 3.2 Data Storage & Persistence (`/tasks.json`)
* Mounts MicroSD card via `SD_MMC.begin()`. If missing or corrupt, fallback to in-memory state and display a warning banner.
* Schema stored in `/tasks.json`:
  ```json
  [
    {
      "id": 1,
      "text": "Submit quarterly report",
      "done": false,
      "created_at": "2026-10-21 09:30"
    }
  ]
  ```
* Any change (new task via HTTP or toggle from physical buttons) flushes directly to `/tasks.json` using `ArduinoJson` (v7).

### 3.3 HTTP REST API Server
A lightweight HTTP server on port 80 with CORS headers enabled:
* `GET /tasks`: Returns the raw JSON array of all tasks (both completed and pending).
* `POST /tasks`: Accepts JSON payload `{"text": "string"}`. Generates incrementing `id`, appends with `"done": false`, timestamps with current NTP time, saves to SD card, and schedules an immediate display refresh.
* `DELETE /tasks/{id}`: Removes a task by ID.
* `POST /tasks/{id}/toggle`: Programmatic completion toggle.

### 3.4 Display & User Interface (ST7305 RLCD)
* **Screen Spec:** 400×300 monochrome 1-bit display. Use low-memory graphics engine (e.g., U8g2 or Waveshare ST7305 native driver).
* **Header Bar:** Current time (`HH:MM`), Wi-Fi status icon/text, and IP address / `screen.local`.
* **Task List View:**
  * Shows tasks with checkboxes: `[ ]` for pending, `[X]` for completed.
  * Clearly highlights the active/selected task cursor (e.g., inverted text row or an arrow indicator `>`).
  * If the list exceeds visible screen height, page or scroll down automatically with the cursor.
* **Footer Bar:** Instructions or status (`[KEY]: Next  [BOOT]: Toggle`).

### 3.5 Physical Button Navigation
* **`KEY` Button (GPIO 18):**
  * Tap: Advance cursor to the next item (wraps to item 0 after the last item).
* **`BOOT` Button (GPIO 0):**
  * Tap: Toggle completion status (`done = !done`) for the currently highlighted task. Persist update to SD card and refresh display.
* **Debounce:** Include a minimum 50ms software debounce to avoid double triggers.

---

## 4. Software Architecture & Concurrency Rules
* **Thread Safety / Dual-Core:** Use FreeRTOS tasks or an event-flag pattern:
  * Network/HTTP server handles incoming requests and parses payloads into a shared task list protected by a FreeRTOS `SemaphoreHandle_t` (mutex).
  * A dirty flag `volatile bool display_needs_update = true;` signals the main display loop to re-render.
* **Non-blocking Loop:** Never block the `loop()` with `delay()`; use `millis()` timers for periodic NTP refreshes and UI clock ticks.

---

## 5. Deliverables Expected
1. `config.h`: Wi-Fi credentials, pin mappings, and server constants.
2. `storage.h` / `storage.cpp`: SD card initialization, JSON serialization, and deserialization routines.
3. `display.h` / `display.cpp`: ST7305 initialization, layout drawing, task list renderer, and cursor management.
4. `network_server.h` / `network_server.cpp`: Wi-Fi, NTP sync, mDNS responder, and HTTP endpoints.
5. `main.ino`: Hardware setup, FreeRTOS tasks/mutex, button interrupts/polling, and main dispatch loop.
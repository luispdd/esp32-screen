#include <Arduino.h>
#include "config.h"
#include "sensors/battery.h"
#include "sensors/environment.h"
#include "storage/task_store.h"
#include "audio/audio_manager.h"
#include "display/display_manager.h"
#include "display/button_handler.h"
#include "network/wifi_manager.h"
#include "network/web_server.h"
#include <esp_log.h>

static const char *TAG = "MAIN";

static void networkTask(void *pvParameters) {
  ESP_LOGI(TAG, "Network task started on Core %d", xPortGetCoreID());
  TodoWebServer::init();

  for (;;) {
    WiFiManager::poll();
    TodoWebServer::poll();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static void uiTask(void *pvParameters) {
  ESP_LOGI(TAG, "UI task started on Core %d", xPortGetCoreID());
  uint32_t lastSensorPoll = 0;

  for (;;) {
    // 1. Poll physical button inputs
    ButtonHandler::poll();

    // 2. Poll ambient environment sensor every 5 seconds
    uint32_t now = millis();
    if (now - lastSensorPoll >= 5000) {
      lastSensorPoll = now;
      float t = 0.0f, h = 0.0f;
      EnvironmentSensor::read(t, h);
    }

    // 3. Render display updates
    DisplayManager::instance().update();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  ESP_LOGI(TAG, "=== Waveshare ESP32-S3 RLCD To-Do Screen Starting ===");

  // Initialize sensors
  BatterySensor::init();
  EnvironmentSensor::init(PIN_I2C_SDA, PIN_I2C_SCL);
  float initialTemp = 0.0f, initialHumi = 0.0f;
  EnvironmentSensor::read(initialTemp, initialHumi);

  // Initialize audio subsystem (ES8311 codec + PA + I2S)
  AudioManager::instance().init();

  // Initialize storage (SDMMC 1-bit / RAM fallback)
  TaskStore::instance().init();

  // Initialize display and buttons
  DisplayManager::instance().init();
  ButtonHandler::init();

  // Initialize Wi-Fi
  WiFiManager::init();

  // Force first render
  DisplayManager::instance().refresh();

  // Core 0: Network and HTTP server
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    8192,
    NULL,
    1,
    NULL,
    0
  );

  // Core 1: Display, buttons, and sensor polling
  xTaskCreatePinnedToCore(
    uiTask,
    "UITask",
    8192,
    NULL,
    1,
    NULL,
    1
  );

  ESP_LOGI(TAG, "All subsystems launched on dual cores");
}

void loop() {
  // FreeRTOS tasks handle all workload on Core 0 and Core 1
  vTaskDelay(pdMS_TO_TICKS(1000));
}

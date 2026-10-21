#include "environment.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_log.h>

static const char *TAG = "ENV_SHTC3";
static const uint8_t SHTC3_ADDR = 0x70;
static const uint16_t CMD_WAKEUP = 0x3517;
static const uint16_t CMD_SLEEP = 0xB098;
static const uint16_t CMD_MEAS_T_RH = 0x7866; // Normal power, Clock stretching disabled, T first

static float s_latest_temp = 0.0f;
static float s_latest_humi = 0.0f;
static bool s_has_valid_reading = false;
static bool s_initialized = false;

static uint8_t calculateCrc(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 8; bit > 0; --bit) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}

static bool sendCommand(uint16_t cmd) {
  Wire.beginTransmission(SHTC3_ADDR);
  Wire.write((uint8_t)(cmd >> 8));
  Wire.write((uint8_t)(cmd & 0xFF));
  return (Wire.endTransmission() == 0);
}

bool EnvironmentSensor::init(int sda, int scl) {
  if (s_initialized) return true;

  if (!Wire.begin(sda, scl, 100000)) {
    ESP_LOGE(TAG, "Wire.begin failed on SDA: %d, SCL: %d", sda, scl);
    return false;
  }

  // Wake up sensor to verify presence
  if (!sendCommand(CMD_WAKEUP)) {
    ESP_LOGW(TAG, "SHTC3 sensor not responding on 0x%02X", SHTC3_ADDR);
    return false;
  }
  delay(1);
  sendCommand(CMD_SLEEP);

  s_initialized = true;
  ESP_LOGI(TAG, "SHTC3 sensor initialized successfully");
  return true;
}

bool EnvironmentSensor::read(float &temperature, float &humidity) {
  if (!s_initialized) {
    if (!init()) {
      return false;
    }
  }

  // 1. Wakeup
  if (!sendCommand(CMD_WAKEUP)) {
    return false;
  }
  delayMicroseconds(300);

  // 2. Start measurement (T first, Normal power, No clock stretching)
  if (!sendCommand(CMD_MEAS_T_RH)) {
    sendCommand(CMD_SLEEP);
    return false;
  }

  // Wait max 15ms for measurement conversion
  delay(15);

  // 3. Read 6 bytes: [Temp_MSB, Temp_LSB, Temp_CRC, Humi_MSB, Humi_LSB, Humi_CRC]
  uint8_t count = Wire.requestFrom((int)SHTC3_ADDR, 6);
  if (count != 6) {
    sendCommand(CMD_SLEEP);
    return false;
  }

  uint8_t raw[6];
  for (int i = 0; i < 6; i++) {
    raw[i] = Wire.read();
  }

  // 4. Enter sleep mode
  sendCommand(CMD_SLEEP);

  // 5. Verify CRCs
  if (calculateCrc(&raw[0], 2) != raw[2]) {
    ESP_LOGW(TAG, "Temperature CRC mismatch");
    return false;
  }
  if (calculateCrc(&raw[3], 2) != raw[5]) {
    ESP_LOGW(TAG, "Humidity CRC mismatch");
    return false;
  }

  uint16_t raw_temp = ((uint16_t)raw[0] << 8) | raw[1];
  uint16_t raw_humi = ((uint16_t)raw[3] << 8) | raw[4];

  // Formulas according to Sensirion SHTC3 datasheet:
  // T [°C] = -45 + 175 * raw_temp / 2^16
  // (Subtract 4.0C PCB self-heating offset calibrated by Waveshare BSP)
  float t = 175.0f * (float)raw_temp / 65536.0f - 45.0f - 4.0f;
  // RH [%] = 100 * raw_humi / 2^16
  float h = 100.0f * (float)raw_humi / 65536.0f;

  if (h < 0.0f) h = 0.0f;
  if (h > 100.0f) h = 100.0f;

  temperature = t;
  humidity = h;
  s_latest_temp = t;
  s_latest_humi = h;
  s_has_valid_reading = true;

  return true;
}

float EnvironmentSensor::getLatestTemperature() {
  return s_latest_temp;
}

float EnvironmentSensor::getLatestHumidity() {
  return s_latest_humi;
}

bool EnvironmentSensor::isValid() {
  return s_has_valid_reading;
}

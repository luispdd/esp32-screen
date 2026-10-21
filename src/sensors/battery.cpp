#include "battery.h"
#include <Arduino.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

static const char *TAG = "BATTERY";
static adc_cali_handle_t s_cali_handle = nullptr;
static adc_oneshot_unit_handle_t s_adc1_handle = nullptr;
static bool s_calibrated = false;
static bool s_initialized = false;

void BatterySensor::init() {
  if (s_initialized) return;

  // 1. Initialize ADC Unit 1
  adc_oneshot_unit_init_cfg_t init_config1 = {};
  init_config1.unit_id = ADC_UNIT_1;
  esp_err_t err = adc_oneshot_new_unit(&init_config1, &s_adc1_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize ADC unit: %d", err);
    return;
  }

  // 2. Configure ADC Channel (ADC1 Channel 3 is GPIO 4 on ESP32-S3)
  adc_oneshot_chan_cfg_t chan_config = {};
  chan_config.bitwidth = ADC_BITWIDTH_12;
  chan_config.atten = ADC_ATTEN_DB_12;
  err = adc_oneshot_config_channel(s_adc1_handle, ADC_CHANNEL_3, &chan_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure ADC channel: %d", err);
    return;
  }

  // 3. Calibration Scheme (Curve Fitting for ESP32-S3)
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  adc_cali_curve_fitting_config_t cali_config = {};
  cali_config.unit_id = ADC_UNIT_1;
  cali_config.chan = ADC_CHANNEL_3;
  cali_config.atten = ADC_ATTEN_DB_12;
  cali_config.bitwidth = ADC_BITWIDTH_12;
  if (adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle) == ESP_OK) {
    s_calibrated = true;
  } else {
    ESP_LOGW(TAG, "ADC curve fitting calibration failed");
  }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  adc_cali_line_fitting_config_t cali_config = {};
  cali_config.unit_id = ADC_UNIT_1;
  cali_config.atten = ADC_ATTEN_DB_12;
  cali_config.bitwidth = ADC_BITWIDTH_12;
  if (adc_cali_create_scheme_line_fitting(&cali_config, &s_cali_handle) == ESP_OK) {
    s_calibrated = true;
  }
#endif

  s_initialized = true;
  ESP_LOGI(TAG, "Battery sensor initialized (calibrated: %d)", s_calibrated);
}

float BatterySensor::getVoltage() {
  if (!s_initialized) {
    init();
  }
  if (!s_adc1_handle) {
    return 0.0f;
  }

  int raw_val = 0;
  esp_err_t err = adc_oneshot_read(s_adc1_handle, ADC_CHANNEL_3, &raw_val);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read ADC raw value: %d", err);
    return 0.0f;
  }

  float vol = 0.0f;
  if (s_calibrated && s_cali_handle) {
    int voltage_mv = 0;
    adc_cali_raw_to_voltage(s_cali_handle, raw_val, &voltage_mv);
    // Voltage divider ratio on board is 3:1 (vol = mV * 0.001 * 3)
    vol = 0.001f * (float)voltage_mv * 3.0f;
  } else {
    // Fallback uncalibrated approximation: 3.3V full scale, 12-bit (4095)
    float pin_voltage = ((float)raw_val / 4095.0f) * 3.3f;
    vol = pin_voltage * 3.0f;
  }

  return vol;
}

uint8_t BatterySensor::getPercentage() {
  float vol = getVoltage();
  if (vol <= 3.0f) {
    return 0;
  }
  if (vol >= 4.12f) {
    return 100;
  }
  float level = ((vol - 3.0f) / 1.12f) * 100.0f;
  if (level > 100.0f) level = 100.0f;
  if (level < 0.0f) level = 0.0f;
  return (uint8_t)level;
}

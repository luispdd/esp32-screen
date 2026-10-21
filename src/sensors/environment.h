#pragma once

#include <stdint.h>

class EnvironmentSensor {
public:
  static bool init(int sda = 13, int scl = 14);
  static bool read(float &temperature, float &humidity);
  static float getLatestTemperature();
  static float getLatestHumidity();
  static bool isValid();
};

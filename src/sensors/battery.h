#pragma once

#include <stdint.h>

class BatterySensor {
public:
  static void init();
  static float getVoltage();
  static uint8_t getPercentage();
};

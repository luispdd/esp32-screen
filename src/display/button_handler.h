#pragma once

#include <Arduino.h>

class ButtonHandler {
public:
  static void init();
  static void poll();
};

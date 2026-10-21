#pragma once

#include <Arduino.h>

class TodoWebServer {
public:
  static void init();
  static void poll();
};

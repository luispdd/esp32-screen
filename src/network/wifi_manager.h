#pragma once

#include <Arduino.h>

class WiFiManager {
public:
  static void init();
  static void poll();
  static bool isConnected();
  static String getCurrentTimestamp();
  static String getLocalIP();
};

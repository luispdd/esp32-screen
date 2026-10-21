#include "wifi_manager.h"
#include "../config.h"
#if __has_include("credentials.h")
  #include "credentials.h"
#elif __has_include("../credentials.h")
  #include "../credentials.h"
#endif
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include <esp_log.h>

static const char *TAG = "WIFI_MGR";
static bool s_mdnsStarted = false;
static bool s_ntpConfigured = false;
static uint32_t s_lastReconnectAttempt = 0;

void WiFiManager::init() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  if (String(WIFI_SSID) != "YOUR_WIFI_SSID" && String(WIFI_SSID).length() > 0) {
    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", WIFI_SSID);
    Serial.printf("[WIFI] Connecting to SSID: '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  } else {
    ESP_LOGW(TAG, "Default Wi-Fi credentials detected. Please configure src/credentials.h");
    Serial.println("[WIFI] Warning: Default Wi-Fi credentials detected. Please check credentials.h");
  }
}

void WiFiManager::poll() {
  uint32_t now = millis();

  static wl_status_t lastStatus = (wl_status_t)99;
  wl_status_t currentStatus = WiFi.status();
  if (currentStatus != lastStatus) {
    lastStatus = currentStatus;
    Serial.printf("[WIFI] Status changed: %d (WL_CONNECTED=3)\n", (int)currentStatus);
  }

  if (currentStatus == WL_CONNECTED) {
    if (!s_mdnsStarted) {
      if (MDNS.begin(MDNS_HOSTNAME)) {
        s_mdnsStarted = true;
        MDNS.addService("http", "tcp", HTTP_SERVER_PORT);
        ESP_LOGI(TAG, "mDNS responder started: %s.local (IP: %s)", MDNS_HOSTNAME, WiFi.localIP().toString().c_str());
        Serial.printf("[WIFI] Connected! IP: %s, Hostname: %s.local\n", WiFi.localIP().toString().c_str(), MDNS_HOSTNAME);
      } else {
        ESP_LOGE(TAG, "Error setting up MDNS responder!");
      }
    }

    if (!s_ntpConfigured) {
      configTzTime(NTP_TIMEZONE, NTP_SERVER, "time.google.com", "time.cloudflare.com");
      s_ntpConfigured = true;
      ESP_LOGI(TAG, "NTP configured with TZ: %s, Server: %s", NTP_TIMEZONE, NTP_SERVER);
      Serial.println("[NTP] Time synchronization configured");
    }
  } else {
    // If disconnected and not configured as default, retry every 10 seconds
    if (String(WIFI_SSID) != "YOUR_WIFI_SSID" && (now - s_lastReconnectAttempt > 10000)) {
      s_lastReconnectAttempt = now;
      ESP_LOGI(TAG, "Reconnecting to Wi-Fi...");
      Serial.println("[WIFI] Attempting reconnect...");
      WiFi.reconnect();
    }
  }
}

bool WiFiManager::isConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

String WiFiManager::getLocalIP() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return "0.0.0.0";
}

String WiFiManager::getCurrentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50)) {
    return "";
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
           timeinfo.tm_year + 1900,
           timeinfo.tm_mon + 1,
           timeinfo.tm_mday,
           timeinfo.tm_hour,
           timeinfo.tm_min);
  return String(buf);
}

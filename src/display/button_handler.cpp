#include "button_handler.h"
#include "display_manager.h"
#include "../config.h"
#include <esp_log.h>

static const char *TAG = "BUTTONS";

struct ButtonState {
  int pin;
  bool isPressed;
  uint32_t pressStartTime;
  bool longPressHandled;
};

static ButtonState s_btnKey = { PIN_BUTTON_KEY, false, 0, false };
static ButtonState s_btnBoot = { PIN_BUTTON_BOOT, false, 0, false };

void ButtonHandler::init() {
  pinMode(PIN_BUTTON_KEY, INPUT_PULLUP);
  pinMode(PIN_BUTTON_BOOT, INPUT_PULLUP);
  ESP_LOGI(TAG, "Button pins configured: KEY=%d, BOOT=%d", PIN_BUTTON_KEY, PIN_BUTTON_BOOT);
}

void ButtonHandler::poll() {
  // Lock out and discard all button inputs while celebration mode is active
  if (DisplayManager::instance().isCelebrationActive()) {
    s_btnKey.isPressed = false;
    s_btnBoot.isPressed = false;
    return;
  }

  uint32_t now = millis();

  // 1. Process KEY Button (Active LOW)
  bool keyReading = (digitalRead(s_btnKey.pin) == LOW);
  if (keyReading && !s_btnKey.isPressed) {
    // Key press started
    s_btnKey.isPressed = true;
    s_btnKey.pressStartTime = now;
    s_btnKey.longPressHandled = false;
  } else if (keyReading && s_btnKey.isPressed) {
    // Key is being held
    uint32_t holdDuration = now - s_btnKey.pressStartTime;
    if (holdDuration >= BUTTON_LONG_PRESS_MS && !s_btnKey.longPressHandled) {
      s_btnKey.longPressHandled = true;
      ESP_LOGI(TAG, "KEY Long-Press: Switch view");
      DisplayManager::instance().toggleView();
    }
  } else if (!keyReading && s_btnKey.isPressed) {
    // Key released
    uint32_t duration = now - s_btnKey.pressStartTime;
    s_btnKey.isPressed = false;
    if (!s_btnKey.longPressHandled && duration >= BUTTON_DEBOUNCE_MS) {
      ESP_LOGI(TAG, "KEY Tap: Next task");
      DisplayManager::instance().nextTask();
    }
  }

  // 2. Process BOOT Button (Active LOW)
  bool bootReading = (digitalRead(s_btnBoot.pin) == LOW);
  if (bootReading && !s_btnBoot.isPressed) {
    // Boot press started
    s_btnBoot.isPressed = true;
    s_btnBoot.pressStartTime = now;
    s_btnBoot.longPressHandled = false;
  } else if (!bootReading && s_btnBoot.isPressed) {
    // Boot released
    uint32_t duration = now - s_btnBoot.pressStartTime;
    s_btnBoot.isPressed = false;
    if (duration >= BUTTON_DEBOUNCE_MS) {
      ESP_LOGI(TAG, "BOOT Tap: Toggle task status");
      DisplayManager::instance().toggleCurrentTask();
    }
  }
}

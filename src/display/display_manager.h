#pragma once

#include <Arduino.h>
#include <vector>
#include "../driver/ST7305_U8g2.h"
#include "../storage/task_store.h"

enum ScreenView {
  VIEW_PENDING = 0,
  VIEW_COMPLETED = 1
};

class DisplayManager {
public:
  static DisplayManager &instance();

  void init();
  void update(bool force = false);
  void refresh();

  void nextTask();
  void toggleView();
  void toggleCurrentTask();

  ScreenView currentView() const;
  int selectedIndex() const;

private:
  DisplayManager();
  ~DisplayManager() = default;
  DisplayManager(const DisplayManager &) = delete;
  DisplayManager &operator=(const DisplayManager &) = delete;

  ST7305_U8g2 _lcd;
  U8G2 *_u8g2;
  ScreenView _currentView;
  int _selectedIndex;
  uint32_t _lastRenderMs;
  uint32_t _lastMinute;

  void drawHeader(const char *timeStr, const char *dateStr, float temp, float humi, bool envValid, uint8_t batPercent, bool hasSd, size_t taskCount);
  void drawTaskList(const std::vector<TaskItem> &tasks);
  void drawFooter();
  String getFormattedTime();
  String getFormattedDate();
};

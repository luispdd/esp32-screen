#include "display_manager.h"
#include "../config.h"
#include "../sensors/battery.h"
#include "../sensors/environment.h"
#include "../network/wifi_manager.h"
#include <time.h>
#include <esp_log.h>

static const char *TAG = "DISP_MGR";

DisplayManager &DisplayManager::instance() {
  static DisplayManager s_instance;
  return s_instance;
}

DisplayManager::DisplayManager()
  : _lcd(PIN_LCD_CLK, PIN_LCD_MOSI, PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_RST),
    _u8g2(nullptr),
    _currentView(VIEW_PENDING),
    _selectedIndex(0),
    _lastRenderMs(0),
    _lastMinute(99) {
}

void DisplayManager::init() {
  _lcd.begin(0, U8G2_R1);
  _u8g2 = _lcd.getU8g2();
  _u8g2->clearBuffer();
  _u8g2->setFont(u8g2_font_7x14_tf);
  _u8g2->drawStr(120, 150, "Booting To-Do Screen...");
  _u8g2->sendBuffer();
  ESP_LOGI(TAG, "ST7305 U8g2 display initialized");
}

String DisplayManager::getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    return "--:--";
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  return String(buf);
}

String DisplayManager::getFormattedDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    return "--/--";
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d/%02d", timeinfo.tm_mday, timeinfo.tm_mon + 1);
  return String(buf);
}

void DisplayManager::refresh() {
  update(true);
}

void DisplayManager::nextTask() {
  auto &store = TaskStore::instance();
  std::vector<TaskItem> tasks = (_currentView == VIEW_PENDING) 
                                ? store.getPendingTasks() 
                                : store.getCompletedTasks();
  if (tasks.empty()) {
    _selectedIndex = 0;
  } else {
    _selectedIndex = (_selectedIndex + 1) % tasks.size();
  }
  update(true);
}

void DisplayManager::toggleView() {
  _currentView = (_currentView == VIEW_PENDING) ? VIEW_COMPLETED : VIEW_PENDING;
  _selectedIndex = 0;
  update(true);
}

void DisplayManager::toggleCurrentTask() {
  auto &store = TaskStore::instance();
  std::vector<TaskItem> tasks = (_currentView == VIEW_PENDING) 
                                ? store.getPendingTasks() 
                                : store.getCompletedTasks();
  if (tasks.empty() || _selectedIndex >= (int)tasks.size()) {
    return;
  }

  uint32_t targetId = tasks[_selectedIndex].id;
  store.toggleTask(targetId);

  // Read updated list to bound selectedIndex
  std::vector<TaskItem> updatedTasks = (_currentView == VIEW_PENDING) 
                                       ? store.getPendingTasks() 
                                       : store.getCompletedTasks();
  if (_selectedIndex >= (int)updatedTasks.size()) {
    _selectedIndex = updatedTasks.empty() ? 0 : (int)updatedTasks.size() - 1;
  }
  update(true);
}

ScreenView DisplayManager::currentView() const {
  return _currentView;
}

int DisplayManager::selectedIndex() const {
  return _selectedIndex;
}

void DisplayManager::drawHeader(const char *timeStr, const char *dateStr, float temp, float humi, bool envValid, uint8_t batPercent, bool hasSd, size_t taskCount) {
  // ==========================================
  // TOP-LEFT: Giant Time taking full header height
  // ==========================================
  _u8g2->setFont(u8g2_font_logisoso50_tf);
  _u8g2->drawStr(8, 55, timeStr);

  // ==========================================
  // TOP-CENTER: 3 lines (Date, Temp, Humidity) - Right-aligned in its container
  // ==========================================
  _u8g2->setFont(u8g2_font_helvB12_tf);
  const int centerRightX = 248;

  // 1. Top line: Date (DD/MM)
  int dateW = _u8g2->getStrWidth(dateStr);
  _u8g2->drawStr(centerRightX - dateW, 18, dateStr);

  // 2. Middle line: Temperature
  char tempBuf[16];
  if (envValid) {
    snprintf(tempBuf, sizeof(tempBuf), "%.1f\xb0\x43", temp);
  } else {
    snprintf(tempBuf, sizeof(tempBuf), "--.-\xb0\x43");
  }
  int tempW = _u8g2->getStrWidth(tempBuf);
  _u8g2->drawStr(centerRightX - tempW, 36, tempBuf);

  // 3. Bottom line: Humidity
  char humiBuf[16];
  if (envValid) {
    snprintf(humiBuf, sizeof(humiBuf), "%d%%", (int)humi);
  } else {
    snprintf(humiBuf, sizeof(humiBuf), "--%%");
  }
  int humiW = _u8g2->getStrWidth(humiBuf);
  _u8g2->drawStr(centerRightX - humiW, 54, humiBuf);

  // ==========================================
  // TOP-RIGHT: Hostname, Battery, KEY/BOOT in two lines
  // ==========================================
  _u8g2->setFont(u8g2_font_6x12_tf);
  const int rightX = 265;

  // Line 1 (y = 14): Network / Hostname & Connection State
  if (WiFiManager::isConnected()) {
    if (!hasSd) {
      _u8g2->drawStr(rightX, 14, "screen.local [NO SD]");
    } else {
      _u8g2->drawStr(rightX, 14, "screen.local (Online)");
    }
  } else {
    _u8g2->drawStr(rightX, 14, "screen.local (Conn...)");
  }

  // Line 2 (y = 28): Battery level with icon
  char batBuf[24];
  snprintf(batBuf, sizeof(batBuf), "Battery: %u%%", batPercent);
  _u8g2->drawStr(rightX, 28, batBuf);
  _u8g2->drawFrame(365, 19, 16, 9);
  _u8g2->drawVLine(381, 21, 5);
  uint8_t barW = (batPercent * 12) / 100;
  if (barW > 0) {
    _u8g2->drawBox(367, 21, barW, 5);
  }

  // Line 3 (y = 42): KEY button info
  _u8g2->drawStr(rightX, 42, "KEY: Next / View");

  // Line 4 (y = 56): BOOT button info
  _u8g2->drawStr(rightX, 56, "BOOT: Toggle");
}

void DisplayManager::drawTaskList(const std::vector<TaskItem> &tasks) {
  if (tasks.empty()) {
    _u8g2->setFont(u8g2_font_helvB18_tf);
    const char *emptyMsg = (_currentView == VIEW_PENDING) 
                           ? "(No pending tasks)" 
                           : "(No completed tasks)";
    int msgWidth = _u8g2->getStrWidth(emptyMsg);
    _u8g2->drawStr((LCD_WIDTH - msgWidth) / 2, 175, emptyMsg);
    return;
  }

  // Bounds check selection
  if (_selectedIndex >= (int)tasks.size()) {
    _selectedIndex = tasks.size() - 1;
  }
  if (_selectedIndex < 0) {
    _selectedIndex = 0;
  }

  // Scrolling window logic for up to 6 visible rows
  const int maxVisible = VISIBLE_TASK_ROWS;
  int windowStart = 0;
  if (_selectedIndex >= maxVisible) {
    windowStart = _selectedIndex - maxVisible + 1;
  }
  int windowEnd = windowStart + maxVisible;
  if (windowEnd > (int)tasks.size()) {
    windowEnd = tasks.size();
  }

  const int startY = 92;
  const int rowHeight = 38;

  for (int i = windowStart; i < windowEnd; i++) {
    int rowIdx = i - windowStart;
    int y = startY + (rowIdx * rowHeight);

    bool isSelected = (i == _selectedIndex);

    // Active Cursor indicator & highlight frame
    if (isSelected) {
      _u8g2->setFont(u8g2_font_helvB18_tf);
      _u8g2->drawStr(6, y, ">");
      _u8g2->drawFrame(2, y - 22, LCD_WIDTH - 4, 32);
    }

    // Status Checkbox in double-size font
    _u8g2->setFont(u8g2_font_helvB18_tf);
    if (tasks[i].done) {
      _u8g2->drawStr(24, y, "[X]");
    } else {
      _u8g2->drawStr(24, y, "[ ]");
    }

    // Task Text (in double-size font)
    String text = tasks[i].text;
    if (text.length() > 25) {
      text = text.substring(0, 22) + "...";
    }
    _u8g2->drawStr(68, y, text.c_str());
  }

  // Scroll indicators if list is longer than visible window
  if (tasks.size() > maxVisible) {
    _u8g2->setFont(u8g2_font_6x12_tf);
    if (windowStart > 0) {
      _u8g2->drawStr(LCD_WIDTH - 12, 74, "^");
    }
    if (windowEnd < (int)tasks.size()) {
      _u8g2->drawStr(LCD_WIDTH - 12, 296, "v");
    }
  }
}

void DisplayManager::drawFooter() {
  // Buttons info is integrated into the top-right header
}

void DisplayManager::update(bool force) {
  uint32_t now = millis();
  auto &store = TaskStore::instance();

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo, 0);
  uint32_t currentMinute = hasTime ? timeinfo.tm_min : 99;

  // Check if redraw is needed: forced, store dirty, or minute changed
  bool minuteChanged = (currentMinute != _lastMinute);
  if (!force && !store.isDirty() && !minuteChanged && (now - _lastRenderMs < 1000)) {
    return;
  }

  _lastRenderMs = now;
  _lastMinute = currentMinute;
  store.clearDirty();

  std::vector<TaskItem> tasks = (_currentView == VIEW_PENDING) 
                                ? store.getPendingTasks() 
                                : store.getCompletedTasks();

  String timeStr = getFormattedTime();
  String dateStr = getFormattedDate();
  float temp = EnvironmentSensor::getLatestTemperature();
  float humi = EnvironmentSensor::getLatestHumidity();
  bool envValid = EnvironmentSensor::isValid();
  uint8_t bat = BatterySensor::getPercentage();
  bool hasSd = store.hasSdCard();

  _u8g2->clearBuffer();
  drawHeader(timeStr.c_str(), dateStr.c_str(), temp, humi, envValid, bat, hasSd, tasks.size());
  drawTaskList(tasks);
  drawFooter();
  _u8g2->sendBuffer();
}

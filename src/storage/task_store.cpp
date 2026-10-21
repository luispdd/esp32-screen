#include "task_store.h"
#include "../config.h"
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include <sys/stat.h>
#include <esp_log.h>

static const char *TAG = "TASK_STORE";

TaskStore &TaskStore::instance() {
  static TaskStore s_instance;
  return s_instance;
}

TaskStore::TaskStore()
  : _mutex(nullptr), _nextId(1), _hasSd(false), _dirty(true) {
  _mutex = xSemaphoreCreateMutex();
}

TaskStore::~TaskStore() {
  if (_mutex) {
    vSemaphoreDelete(_mutex);
  }
}

void TaskStore::lock() {
  if (_mutex) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
  }
}

void TaskStore::unlock() {
  if (_mutex) {
    xSemaphoreGive(_mutex);
  }
}

bool TaskStore::hasSdCard() const {
  return _hasSd;
}

bool TaskStore::isDirty() {
  return _dirty;
}

void TaskStore::clearDirty() {
  _dirty = false;
}

bool TaskStore::init() {
  lock();

  // Initialize SDMMC 1-Bit mode
  // Pins: CLK=38, CMD=21, D0=39
  bool sdMounted = false;
  if (SD_MMC.setPins(PIN_SDMMC_CLK, PIN_SDMMC_CMD, PIN_SDMMC_D0)) {
    sdMounted = SD_MMC.begin(SDMMC_MOUNT_POINT, true /* 1-bit mode */);
  }

  if (sdMounted) {
    _hasSd = true;
    ESP_LOGI(TAG, "SD Card mounted successfully (Card size: %llu MB)", SD_MMC.cardSize() / (1024 * 1024));

    // Ensure directory exists
    struct stat st;
    if (stat(TASKS_DIR, &st) != 0) {
      if (mkdir(TASKS_DIR, 0755) == 0) {
        ESP_LOGI(TAG, "Created directory %s", TASKS_DIR);
      } else {
        ESP_LOGE(TAG, "Failed to create directory %s", TASKS_DIR);
      }
    }

    unlock();
    loadFromFile();
  } else {
    _hasSd = false;
    ESP_LOGW(TAG, "SD Card mount failed or missing. Operating in RAM-only mode.");
    unlock();
  }

  lock();
  if (_tasks.empty()) {
    TaskItem sample1;
    sample1.id = _nextId++;
    sample1.text = "Welcome to ESP32 Screen!";
    sample1.done = false;
    sample1.created_at = "Initial";
    _tasks.push_back(sample1);
    _dirty = true;
  }
  unlock();

  return true;
}

std::vector<TaskItem> TaskStore::getAllTasks() {
  lock();
  std::vector<TaskItem> copy = _tasks;
  unlock();
  return copy;
}

std::vector<TaskItem> TaskStore::getPendingTasks() {
  lock();
  std::vector<TaskItem> result;
  for (const auto &item : _tasks) {
    if (!item.done) {
      result.push_back(item);
    }
  }
  unlock();
  return result;
}

std::vector<TaskItem> TaskStore::getCompletedTasks() {
  lock();
  std::vector<TaskItem> result;
  for (const auto &item : _tasks) {
    if (item.done) {
      result.push_back(item);
    }
  }
  unlock();
  return result;
}

bool TaskStore::getTaskById(uint32_t id, TaskItem &outTask) {
  lock();
  for (const auto &item : _tasks) {
    if (item.id == id) {
      outTask = item;
      unlock();
      return true;
    }
  }
  unlock();
  return false;
}

std::vector<TaskItem> TaskStore::addTasks(const std::vector<String> &texts, const String &timestamp) {
  lock();
  std::vector<TaskItem> created;
  for (const auto &txt : texts) {
    TaskItem item;
    item.id = _nextId++;
    item.text = txt;
    item.done = false;
    item.created_at = timestamp;
    _tasks.push_back(item);
    created.push_back(item);
  }
  _dirty = true;
  unlock();

  saveToFile();
  return created;
}

bool TaskStore::toggleTask(uint32_t id, TaskItem *outUpdated) {
  lock();
  bool found = false;
  for (auto &item : _tasks) {
    if (item.id == id) {
      item.done = !item.done;
      if (outUpdated) {
        *outUpdated = item;
      }
      found = true;
      break;
    }
  }
  if (found) {
    _dirty = true;
  }
  unlock();

  if (found) {
    saveToFile();
  }
  return found;
}

bool TaskStore::deleteTask(uint32_t id) {
  lock();
  bool found = false;
  for (auto it = _tasks.begin(); it != _tasks.end(); ++it) {
    if (it->id == id) {
      _tasks.erase(it);
      found = true;
      _dirty = true;
      break;
    }
  }
  unlock();

  if (found) {
    saveToFile();
  }
  return found;
}

bool TaskStore::saveToFile() {
  if (!_hasSd) {
    return false;
  }

  lock();
  std::vector<TaskItem> tasksCopy = _tasks;
  unlock();

  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (const auto &item : tasksCopy) {
    JsonObject obj = array.add<JsonObject>();
    obj["id"] = item.id;
    obj["text"] = item.text;
    obj["done"] = item.done;
    obj["created_at"] = item.created_at;
  }

  String jsonStr;
  if (serializeJson(doc, jsonStr) == 0) {
    ESP_LOGE(TAG, "Failed to serialize JSON");
    return false;
  }

  // Atomic write via temporary file
  FILE *f = fopen(TASKS_TMP_PATH, "w");
  if (!f) {
    ESP_LOGE(TAG, "Failed to open temporary file for write: %s", TASKS_TMP_PATH);
    return false;
  }

  size_t written = fwrite(jsonStr.c_str(), 1, jsonStr.length(), f);
  fflush(f);
  fsync(fileno(f));
  fclose(f);

  if (written != jsonStr.length()) {
    ESP_LOGE(TAG, "Failed to write complete JSON data");
    unlink(TASKS_TMP_PATH);
    return false;
  }

  // Replace active file atomically
  unlink(TASKS_FILE_PATH);
  if (rename(TASKS_TMP_PATH, TASKS_FILE_PATH) != 0) {
    ESP_LOGE(TAG, "Failed to rename %s to %s", TASKS_TMP_PATH, TASKS_FILE_PATH);
    return false;
  }

  ESP_LOGI(TAG, "Successfully persisted %u tasks to %s", (unsigned)tasksCopy.size(), TASKS_FILE_PATH);
  return true;
}

bool TaskStore::loadFromFile() {
  if (!_hasSd) {
    return false;
  }

  FILE *f = fopen(TASKS_FILE_PATH, "r");
  if (!f) {
    ESP_LOGW(TAG, "Tasks file %s not found. Will be created on next save.", TASKS_FILE_PATH);
    return false;
  }

  fseek(f, 0, SEEK_END);
  long fileSize = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (fileSize <= 0) {
    fclose(f);
    return false;
  }

  std::string content;
  content.resize(fileSize);
  size_t readBytes = fread(&content[0], 1, fileSize, f);
  fclose(f);

  if (readBytes == 0) {
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, content);
  if (err) {
    ESP_LOGE(TAG, "JSON deserialization error from %s: %s", TASKS_FILE_PATH, err.c_str());
    return false;
  }

  if (!doc.is<JsonArray>()) {
    ESP_LOGE(TAG, "Root JSON in %s is not an array", TASKS_FILE_PATH);
    return false;
  }

  lock();
  _tasks.clear();
  uint32_t maxId = 0;
  for (JsonObject item : doc.as<JsonArray>()) {
    TaskItem task;
    task.id = item["id"] | 0;
    task.text = (const char*)(item["text"] | "");
    task.done = item["done"] | false;
    task.created_at = (const char*)(item["created_at"] | "");

    if (task.id > maxId) {
      maxId = task.id;
    }
    _tasks.push_back(task);
  }
  _nextId = maxId + 1;
  _dirty = true;
  unlock();

  ESP_LOGI(TAG, "Loaded %u tasks from %s (next ID: %u)", (unsigned)_tasks.size(), TASKS_FILE_PATH, (unsigned)_nextId);
  return true;
}

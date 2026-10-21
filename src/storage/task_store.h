#pragma once

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct TaskItem {
  uint32_t id;
  String text;
  bool done;
  String created_at;
};

class TaskStore {
public:
  static TaskStore &instance();

  bool init();
  bool hasSdCard() const;

  std::vector<TaskItem> getAllTasks();
  std::vector<TaskItem> getPendingTasks();
  std::vector<TaskItem> getCompletedTasks();
  bool getTaskById(uint32_t id, TaskItem &outTask);

  std::vector<TaskItem> addTasks(const std::vector<String> &texts, const String &timestamp = "");
  bool toggleTask(uint32_t id, TaskItem *outUpdated = nullptr);
  bool deleteTask(uint32_t id);

  bool saveToFile();
  bool loadFromFile();

  bool isDirty();
  void clearDirty();

private:
  TaskStore();
  ~TaskStore();
  TaskStore(const TaskStore &) = delete;
  TaskStore &operator=(const TaskStore &) = delete;

  SemaphoreHandle_t _mutex;
  std::vector<TaskItem> _tasks;
  uint32_t _nextId;
  bool _hasSd;
  volatile bool _dirty;

  void lock();
  void unlock();
};

#include "web_server.h"
#include "wifi_manager.h"
#include "../config.h"
#include "../storage/task_store.h"
#include "../display/display_manager.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_log.h>

static const char *TAG = "WEB_SERVER";
static WebServer s_server(HTTP_SERVER_PORT);

static void sendCorsHeaders() {
  s_server.sendHeader("Access-Control-Allow-Origin", "*");
  s_server.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  s_server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleCorsOptions() {
  sendCorsHeaders();
  s_server.send(204, "text/plain", "");
}

static void handleGetTasks() {
  sendCorsHeaders();
  auto &store = TaskStore::instance();
  std::vector<TaskItem> tasks = store.getAllTasks();

  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (const auto &item : tasks) {
    JsonObject obj = array.add<JsonObject>();
    obj["id"] = item.id;
    obj["text"] = item.text;
    obj["done"] = item.done;
    obj["created_at"] = item.created_at;
  }

  String response;
  serializeJson(doc, response);
  s_server.send(200, "application/json", response);
}

static void handlePostTasks() {
  sendCorsHeaders();

  if (!s_server.hasArg("plain")) {
    s_server.send(400, "application/json", "{\"error\": \"Request body is empty\"}");
    return;
  }

  String body = s_server.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err) {
    s_server.send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
    return;
  }

  // Strict requirement: Payload must be a JSON array
  if (!doc.is<JsonArray>()) {
    s_server.send(400, "application/json", "{\"error\": \"Payload must be a JSON array of tasks\"}");
    return;
  }

  JsonArray array = doc.as<JsonArray>();
  if (array.size() == 0) {
    s_server.send(400, "application/json", "{\"error\": \"Array must contain at least one task\"}");
    return;
  }

  std::vector<String> taskTexts;
  for (JsonObject item : array) {
    if (item["text"].is<const char*>()) {
      const char *t = item["text"].as<const char*>();
      if (t && strlen(t) > 0) {
        taskTexts.push_back(String(t));
      }
    }
  }

  if (taskTexts.empty()) {
    s_server.send(400, "application/json", "{\"error\": \"No valid task text found in array\"}");
    return;
  }

  String timestamp = WiFiManager::getCurrentTimestamp();
  if (timestamp.length() == 0) {
    timestamp = "Pending Sync";
  }

  auto &store = TaskStore::instance();
  std::vector<TaskItem> created = store.addTasks(taskTexts, timestamp);
  DisplayManager::instance().refresh();

  JsonDocument resDoc;
  JsonArray resArray = resDoc.to<JsonArray>();
  for (const auto &item : created) {
    JsonObject obj = resArray.add<JsonObject>();
    obj["id"] = item.id;
    obj["text"] = item.text;
    obj["done"] = item.done;
    obj["created_at"] = item.created_at;
  }

  String response;
  serializeJson(resDoc, response);
  s_server.send(201, "application/json", response);
}

static void handleNotFound() {
  if (s_server.method() == HTTP_OPTIONS) {
    handleCorsOptions();
    return;
  }

  sendCorsHeaders();
  String uri = s_server.uri();

  // Pattern match: /tasks/{id}/toggle
  if (uri.startsWith("/tasks/") && uri.endsWith("/toggle") && s_server.method() == HTTP_POST) {
    String idStr = uri.substring(7, uri.length() - 7);
    uint32_t id = (uint32_t)idStr.toInt();
    if (id == 0 && idStr != "0") {
      s_server.send(400, "application/json", "{\"error\": \"Invalid task ID\"}");
      return;
    }

    auto &store = TaskStore::instance();
    TaskItem updated;
    if (store.toggleTask(id, &updated)) {
      DisplayManager::instance().refresh();
      JsonDocument doc;
      doc["id"] = updated.id;
      doc["text"] = updated.text;
      doc["done"] = updated.done;
      doc["created_at"] = updated.created_at;

      String response;
      serializeJson(doc, response);
      s_server.send(200, "application/json", response);
    } else {
      s_server.send(404, "application/json", "{\"error\": \"Task not found\"}");
    }
    return;
  }

  // Pattern match: DELETE /tasks/{id}
  if (uri.startsWith("/tasks/") && s_server.method() == HTTP_DELETE) {
    String idStr = uri.substring(7);
    uint32_t id = (uint32_t)idStr.toInt();
    if (id == 0 && idStr != "0") {
      s_server.send(400, "application/json", "{\"error\": \"Invalid task ID\"}");
      return;
    }

    auto &store = TaskStore::instance();
    if (store.deleteTask(id)) {
      DisplayManager::instance().refresh();
      s_server.send(200, "application/json", "{\"status\": \"deleted\"}");
    } else {
      s_server.send(404, "application/json", "{\"error\": \"Task not found\"}");
    }
    return;
  }

  s_server.send(404, "application/json", "{\"error\": \"Endpoint not found\"}");
}

void TodoWebServer::init() {
  s_server.on("/tasks", HTTP_GET, handleGetTasks);
  s_server.on("/tasks", HTTP_POST, handlePostTasks);
  s_server.on("/tasks", HTTP_OPTIONS, handleCorsOptions);
  s_server.onNotFound(handleNotFound);

  s_server.begin();
  ESP_LOGI(TAG, "HTTP REST server listening on port %d", HTTP_SERVER_PORT);
}

void TodoWebServer::poll() {
  s_server.handleClient();
}

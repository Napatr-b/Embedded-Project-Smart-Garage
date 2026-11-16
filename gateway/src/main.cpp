#include "config.h"
#include "door_controller.h"
#include "hardware.h"
#include "network.h"
#include "protocol.h"
#include "pins.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cstring>
using namespace garage;
static DoorController door(GARAGE_SIMULATED_HARDWARE);
static CommandGuard guard;
static bool online = false;
static int64_t sessionStart = 0;
static void send(const char *topic, cJSON *p, bool retain = false) {
  if (!p)
    return;
  char *json = cJSON_PrintUnformatted(p);
  if (json) {
    network::publish(topic, json, retain);
    free(json);
  }
  cJSON_Delete(p);
}
static void ack(const char *id, const char *status, const char *reason = nullptr) {
  if (!id || !id[0])
    return;
  auto *p = envelope();
  cJSON_AddStringToObject(p, "commandId", id);
  cJSON_AddStringToObject(p, "status", status);
  if (reason)
    cJSON_AddStringToObject(p, "reason", reason);
  send("ack", p);
}
static void event(const char *type, const char *message) {
  ESP_LOGI("garage", "%s: %s", type, message);
  if (!online)
    return;
  auto *p = envelope();
  cJSON_AddStringToObject(p, "type", type);
  cJSON_AddStringToObject(p, "message", message);
  send("event", p);
}
static void number(cJSON *p, const char *name, double value) {
  if (std::isfinite(value))
    cJSON_AddNumberToObject(p, name, value);
  else
    cJSON_AddNullToObject(p, name);
}
static void report(int64_t ms, const char *confirmedCommand = nullptr) {
  auto *status = envelope();
  cJSON_AddBoolToObject(status, "online", true);
  send("status", status, true);
  auto s = sampleHardware(GARAGE_SIMULATED_HARDWARE, ms);
  auto *p = envelope();
  const char *name = doorName(door.state());
  if (name)
    cJSON_AddStringToObject(p, "door", name);
  else
    cJSON_AddNullToObject(p, "door");
  if (GARAGE_SIMULATED_HARDWARE) cJSON_AddNumberToObject(p, "doorPosition", door.position());
  else cJSON_AddNullToObject(p, "doorPosition");
  number(p, "temperature", s.temperature);
  number(p, "humidity", s.humidity);
  number(p, "ldrRaw", s.ldrRaw);
  number(p, "aqi", s.aqi);
  number(p, "tvocPpb", s.tvocPpb);
  number(p, "eco2Ppm", s.eco2Ppm);
  number(p, "distanceCm", s.distanceCm);
  if (s.occupied < 0)
    cJSON_AddNullToObject(p, "occupied");
  else
    cJSON_AddBoolToObject(p, "occupied", s.occupied);
  if (s.ledOn < 0)
    cJSON_AddNullToObject(p, "ledOn");
  else
    cJSON_AddBoolToObject(p, "ledOn", s.ledOn);
  if (confirmedCommand)
    cJSON_AddStringToObject(p, "commandId", confirmedCommand);
  else if (door.pendingId()[0])
    cJSON_AddStringToObject(p, "commandId", door.pendingId());
  else
    cJSON_AddNullToObject(p, "commandId");
  cJSON_AddNullToObject(p, "commandedServoAngle");
  cJSON_AddBoolToObject(p, "positionConfirmed", GARAGE_SIMULATED_HARDWARE);
  cJSON_AddBoolToObject(p, "actuatorEnabled", false);
  send("telemetry", p);
}
extern "C" void app_main() {
  setenv("TZ", "UTC0", 1);
  tzset();
  auto nvs = nvs_flash_init();
  if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGE("garage", "NVS needs maintenance; not erasing stored data automatically");
    return;
  }
  ESP_ERROR_CHECK(nvs);
  uint32_t flash = 0;
  esp_flash_get_size(nullptr, &flash);
  const size_t psram = esp_psram_is_initialized() ? esp_psram_get_size() : 0;
  ESP_LOGI("garage", "Smart Garage | ESP-IDF %s | schema %d", esp_get_idf_version(),
           kSchemaVersion);
  ESP_LOGI("garage", "Flash=%lu bytes expected=%u; PSRAM=%u bytes expected=8388608",
           static_cast<unsigned long>(flash), unsigned(GARAGE_EXPECTED_FLASH_BYTES), static_cast<unsigned>(psram));
  if (flash != GARAGE_EXPECTED_FLASH_BYTES || psram != 8388608) {
    ESP_LOGE("garage", "MEMORY MISMATCH: verify module label, power and Octal sdkconfig before "
                       "hardware integration");
    return; // Do not start network/commands when physical memory does not match.
  }
  ESP_LOGW("garage", "MODE=%s | REAL ACTUATOR DISABLED | ESP-WHO DISABLED | GPIO UNASSIGNED",
           GARAGE_SIMULATED_HARDWARE ? "SIMULATED_HARDWARE" : "BRINGUP_OR_MQTT_NO_HARDWARE");
  QueueHandle_t queue = xQueueCreate(8, sizeof(NetworkMessage));
  if (!queue) {
    ESP_LOGE("garage", "Queue allocation failed");
    return;
  }
  network::begin(queue);
  int64_t lastReport = -3000, lastSerial = -3000;
  while (true) {
    const int64_t mono = esp_timer_get_time() / 1000;
    network::poll(mono);
    if (!network::mqttConnected())
      online = false;
    NetworkMessage msg{};
    while (xQueueReceive(queue, &msg, 0) == pdTRUE) {
      if (msg.kind == NetworkMessage::Connected) {
        online = network::mqttConnected();
        if (!online)
          continue;
        sessionStart = utcMs();
        event("CONNECTED", "Gateway connected; no command replay; physical actuator disabled");
        lastReport = -3000;
      } else if (msg.kind == NetworkMessage::Disconnected) {
        online = false;
      } else if (msg.kind == NetworkMessage::CommandData) {
        Command c{};
        const char *invalid = parseCommand(msg.payload, msg.length, c);
        if (invalid) {
          if (c.id[0])
            ack(c.id, "rejected", invalid);
          event("INVALID_COMMAND", invalid);
          continue;
        }
        ack(c.id, "received");
        const char *reason = guard.check(c, msg.retained, clockSynced(), utcMs());
        if (!reason && (!online || c.sentMs < sessionStart))
          reason = "not_current_connection";
        if (reason) {
          ack(c.id, "rejected", reason);
          continue;
        }
        guard.remember(c);
        if (GARAGE_SIMULATED_HARDWARE && door.state() != Door::Fault && door.pendingId()[0]) {
          ack(door.pendingId(), "rejected", "superseded_by_command");
        }
        reason = door.request(c, mono);
        if (reason) {
          ack(c.id, "rejected", reason);
          continue;
        }
        ack(c.id, "accepted");
        event("COMMAND_ACCEPTED", c.action);
        if (!strcmp(c.action, "stop")) {
          ack(c.id, "completed", "simulated_stopped");
          report(mono, c.id);
        } else
          report(mono);
      }
    }
    if (const char *result = door.tick(mono, online)) {
      if (online) {
        ack(door.pendingId(), !strcmp(result, "completed") ? "completed" : "rejected", result);
        report(mono);
      }
      event("MOVEMENT", result);
      door.clearPending();
    }
    if (mono - lastSerial >= 3000) {
      lastSerial = mono;
      ESP_LOGI("garage", "Heartbeat uptime=%lld ms wifi=%s mqtt=%s timeSynced=%s door=%s heap=%u",
               static_cast<long long>(mono), network::wifiConnected() ? "online" : "offline",
               online ? "online" : "offline", clockSynced() ? "yes" : "no",
               doorName(door.state()) ? doorName(door.state()) : "Unknown",
               static_cast<unsigned>(esp_get_free_heap_size()));
    }
    if (online && mono - lastReport >= kHeartbeatMs) {
      lastReport = mono;
      report(mono);
    }
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

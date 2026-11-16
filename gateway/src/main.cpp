// Smart Garage Gateway (ESP32-S3 N16R8, ESP-IDF)
//
// Each feature lives in its own file:
//   node_link.cpp        UART from the sensor node: frames, ACK, dedupe, loss count (own task)
//   hardware.cpp         one SensorSnapshot: simulated values or real node data
//   door_controller.cpp  door state machine, movement timeout, command replay guard
//   network.cpp          Wi-Fi, SNTP, MQTT over TLS, Last Will
//   protocol.cpp         JSON envelope + command parsing/validation
//   reporting.cpp        publishes telemetry / status / ack / event
// This file: startup checks, then the main loop (network events, commands, door tick, heartbeat).
#include "config.h"
#include "door_controller.h"
#include "network.h"
#include "node_link.h"
#include "protocol.h"
#include "reporting.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
using namespace garage;
static DoorController door(GARAGE_SIMULATED_HARDWARE);
static CommandGuard guard;
static bool online = false;
static int64_t sessionStart = 0;

static void event(const char *type, const char *message) { reporting::event(type, message, online); }

// Validates a dashboard command and hands it to the door. Every outcome is ACKed.
static void handleCommand(const NetworkMessage &msg, int64_t mono) {
  Command c{};
  const char *invalid = parseCommand(msg.payload, msg.length, c);
  if (invalid) {
    if (c.id[0])
      reporting::ack(c.id, "rejected", invalid);
    event("INVALID_COMMAND", invalid);
    return;
  }
  reporting::ack(c.id, "received");
  const char *reason = guard.check(c, msg.retained, clockSynced(), utcMs());
  if (!reason && (!online || c.sentMs < sessionStart))
    reason = "not_current_connection";
  if (reason) {
    reporting::ack(c.id, "rejected", reason);
    return;
  }
  guard.remember(c);
  if (GARAGE_SIMULATED_HARDWARE && door.state() != Door::Fault && door.pendingId()[0])
    reporting::ack(door.pendingId(), "rejected", "superseded_by_command");
  reason = door.request(c, mono);
  if (reason) {
    reporting::ack(c.id, "rejected", reason);
    return;
  }
  reporting::ack(c.id, "accepted");
  event("COMMAND_ACCEPTED", c.action);
  if (!strcmp(c.action, "stop")) {
    reporting::ack(c.id, "completed", "simulated_stopped");
    reporting::telemetry(door, mono, c.id);
  } else
    reporting::telemetry(door, mono);
}

// Refuses to start when flash/PSRAM don't match the build, so a wrong board/sdkconfig is caught early.
static bool checkMemory() {
  uint32_t flash = 0;
  esp_flash_get_size(nullptr, &flash);
  const size_t psram = esp_psram_is_initialized() ? esp_psram_get_size() : 0;
  ESP_LOGI("garage", "Smart Garage | ESP-IDF %s | schema %d", esp_get_idf_version(), kSchemaVersion);
  ESP_LOGI("garage", "Flash=%lu bytes expected=%u; PSRAM=%u bytes expected=8388608",
           static_cast<unsigned long>(flash), unsigned(GARAGE_EXPECTED_FLASH_BYTES),
           static_cast<unsigned>(psram));
  if (flash != GARAGE_EXPECTED_FLASH_BYTES || psram != 8388608) {
    ESP_LOGE("garage", "MEMORY MISMATCH: verify module label, power and Octal sdkconfig before "
                       "hardware integration");
    return false;
  }
  return true;
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
  if (!checkMemory())
    return; // Do not start network/commands when physical memory does not match.
  ESP_LOGW("garage", "MODE=%s | REAL ACTUATOR DISABLED | ESP-WHO DISABLED",
           GARAGE_SIMULATED_HARDWARE ? "SIMULATED_HARDWARE" : "REAL_SENSORS_VIA_NODE");

  node_link::begin(); // runs in its own task; works with or without network

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
        handleCommand(msg, mono);
      }
    }

    if (const char *result = door.tick(mono, online)) {
      if (online) {
        reporting::ack(door.pendingId(), !strcmp(result, "completed") ? "completed" : "rejected", result);
        reporting::telemetry(door, mono);
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
      reporting::telemetry(door, mono);
    }
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

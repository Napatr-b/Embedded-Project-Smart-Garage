#include "reporting.h"
#include "config.h"
#include "hardware.h"
#include "network.h"
#include "protocol.h"
#include "esp_log.h"
#include <cmath>
#include <cstdlib>
namespace garage::reporting {
namespace {
// Serialises, publishes and frees p.
void send(const char *topic, cJSON *p, bool retain = false) {
  if (!p)
    return;
  char *json = cJSON_PrintUnformatted(p);
  if (json) {
    network::publish(topic, json, retain);
    free(json);
  }
  cJSON_Delete(p);
}
// Missing readings are NAN and go out as JSON null, so the dashboard shows "no data", not 0.
void number(cJSON *p, const char *name, double value) {
  if (std::isfinite(value))
    cJSON_AddNumberToObject(p, name, value);
  else
    cJSON_AddNullToObject(p, name);
}
void tristate(cJSON *p, const char *name, int value) {
  if (value < 0)
    cJSON_AddNullToObject(p, name);
  else
    cJSON_AddBoolToObject(p, name, value);
}
} // namespace

void ack(const char *id, const char *status, const char *reason) {
  if (!id || !id[0])
    return;
  auto *p = envelope();
  cJSON_AddStringToObject(p, "commandId", id);
  cJSON_AddStringToObject(p, "status", status);
  if (reason)
    cJSON_AddStringToObject(p, "reason", reason);
  send("ack", p);
}

void event(const char *type, const char *message, bool online) {
  ESP_LOGI("garage", "%s: %s", type, message);
  if (!online)
    return;
  auto *p = envelope();
  cJSON_AddStringToObject(p, "type", type);
  cJSON_AddStringToObject(p, "message", message);
  send("event", p);
}

void telemetry(const DoorController &door, int64_t ms, const char *confirmedCommand) {
  auto *status = envelope();
  cJSON_AddBoolToObject(status, "online", true);
  send("status", status, true);

  const auto s = sampleHardware(GARAGE_SIMULATED_HARDWARE, ms);
  auto *p = envelope();
  const char *name = doorName(door.state());
  if (name)
    cJSON_AddStringToObject(p, "door", name);
  else
    cJSON_AddNullToObject(p, "door");
  if (GARAGE_SIMULATED_HARDWARE)
    cJSON_AddNumberToObject(p, "doorPosition", door.position());
  else
    cJSON_AddNullToObject(p, "doorPosition");
  number(p, "temperature", s.temperature);
  number(p, "humidity", s.humidity);
  number(p, "ldrRaw", s.ldrRaw);
  number(p, "aqi", s.aqi);
  number(p, "tvocPpb", s.tvocPpb);
  number(p, "eco2Ppm", s.eco2Ppm);
  number(p, "distanceCm", s.distanceCm);
  tristate(p, "occupied", s.occupied);
  tristate(p, "ledOn", s.ledOn);
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
} // namespace garage::reporting

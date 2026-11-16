#pragma once
// MQTT messages the gateway publishes (topics/payloads: docs/integration-contract.md).
#include <cstdint>
#include "door_controller.h"
namespace garage::reporting {
// Command acknowledgement on <prefix>/<device>/ack.
void ack(const char *commandId, const char *status, const char *reason = nullptr);
// Always logged to serial; published on <prefix>/<device>/event only while online.
void event(const char *type, const char *message, bool online);
// Retained online status + one telemetry sample (door + sensors).
void telemetry(const DoorController &door, int64_t monotonicMs, const char *confirmedCommand = nullptr);
} // namespace garage::reporting

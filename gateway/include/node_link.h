#pragma once
// UART link to the sensor node(s): receives framed messages (shared/protocol/garage_protocol.h),
// ACKs every valid frame, drops duplicate SEQs and counts lost ones (E4).
// Runs its own FreeRTOS task (E1) so a slow MQTT publish never delays an ACK.
#include <cstdint>
#include "hardware.h"
namespace garage::node_link {
// Starts UART1 + the receive task. Returns false if the UART pins are not assigned in pins.h.
bool begin();
// Copies the latest node readings that are fresher than kStaleMs into s. Older data stays NAN/-1.
void fill(SensorSnapshot &s, int64_t monotonicMs);
} // namespace garage::node_link

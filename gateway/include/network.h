#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
namespace garage {
struct NetworkMessage {
  enum Kind { Connected, Disconnected, CommandData } kind;
  bool retained = false;
  char payload[1025]{};
  size_t length = 0;
};
namespace network {
bool wifiConnected();
bool mqttConnected();
void begin(QueueHandle_t queue);
void poll(int64_t monotonicMs);
bool publish(const char *topic, const char *json, bool retain = false);
void topic(char *out, size_t length, const char *suffix);
} // namespace network
} // namespace garage

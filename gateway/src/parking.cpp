#include "parking.h"
#include "config.h"
#include "hardware.h"
#include "network.h"
#include "node_link.h"
#include "protocol.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>
#include <ctime>

#ifndef GARAGE_PARKING_URL
#define GARAGE_PARKING_URL ""
#endif
#ifndef GARAGE_PARKING_TOKEN
#define GARAGE_PARKING_TOKEN ""
#endif
#ifndef GARAGE_PARKING_OCCUPIED_CM
#define GARAGE_PARKING_OCCUPIED_CM 60
#endif
#ifndef GARAGE_PARKING_VACANT_CM
#define GARAGE_PARKING_VACANT_CM 80
#endif
#ifndef GARAGE_PARKING_STABLE_SAMPLES
#define GARAGE_PARKING_STABLE_SAMPLES 3
#endif

namespace garage::parking {
namespace {
static_assert(GARAGE_PARKING_OCCUPIED_CM < GARAGE_PARKING_VACANT_CM);
static_assert(GARAGE_PARKING_STABLE_SAMPLES >= 2);
constexpr const char *tag = "parking";
constexpr int64_t maxSampleGapMs = 2500; // Sensor node normally sends every 1000 ms.
QueueHandle_t events = nullptr;
struct Event {
  uint32_t nonce[4];
  int64_t occurredMs;
  bool occupied;
};

bool send(const Event &event) {
  const time_t seconds = event.occurredMs / 1000;
  tm utc{};
  gmtime_r(&seconds, &utc);
  char timestamp[24];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &utc);
  char body[256];
  const int length = snprintf(body, sizeof(body),
      "{\"event_id\":\"%08lx%08lx%08lx%08lx\",\"gateway_id\":\"%s\","
      "\"occupied\":%s,\"occurred_at\":\"%s.%03dZ\"}",
      static_cast<unsigned long>(event.nonce[0]), static_cast<unsigned long>(event.nonce[1]),
      static_cast<unsigned long>(event.nonce[2]), static_cast<unsigned long>(event.nonce[3]),
      GARAGE_DEVICE_ID, event.occupied ? "true" : "false", timestamp,
      static_cast<int>(event.occurredMs % 1000));
  if (length < 0 || length >= static_cast<int>(sizeof(body)))
    return false;

  esp_http_client_config_t config{};
  config.url = GARAGE_PARKING_URL;
  config.method = HTTP_METHOD_POST;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.disable_auto_redirect = true;
  config.timeout_ms = 15000;
  auto client = esp_http_client_init(&config);
  if (!client)
    return false;
  esp_http_client_set_header(client, "Authorization", "Bearer " GARAGE_PARKING_TOKEN);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, body, length);
  const auto result = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  ESP_LOGI(tag, "Parking POST: HTTP %d (%s)", status, esp_err_to_name(result));
  return result == ESP_OK && (status == 200 || status == 201);
}

void upload(void *) {
  Event event{};
  for (;;) {
    if (xQueueReceive(events, &event, portMAX_DELAY) != pdTRUE)
      continue;
    while (!network::wifiConnected() || !send(event))
      vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
} // namespace

void begin() {
  if (!GARAGE_NETWORK_ENABLED || GARAGE_SIMULATED_HARDWARE ||
      strncmp(GARAGE_PARKING_URL, "https://", 8) || !strlen(GARAGE_PARKING_TOKEN))
    return;
  events = xQueueCreate(16, sizeof(Event));
  if (events && xTaskCreate(upload, "parking_https", 8192, nullptr, 3, nullptr) == pdPASS)
    return;
  if (events)
    vQueueDelete(events);
  events = nullptr;
  ESP_LOGE(tag, "Could not start parking uploader");
}

void poll(int64_t monotonicMs) {
  if (!events || !clockSynced())
    return;
  static int64_t lastSample = -1;
  static int current = -1, candidate = -1, count = 0;
  SensorSnapshot sample;
  node_link::fill(sample, monotonicMs);
  if (sample.ultrasonicSampleMs < 0 || sample.ultrasonicSampleMs == lastSample)
    return;
  if (sample.ultrasonicSampleMs - lastSample > maxSampleGapMs)
    count = 0;
  lastSample = sample.ultrasonicSampleMs;

  int observed = -1;
  if (std::isfinite(sample.distanceCm) && sample.distanceCm > 0) {
    if (sample.distanceCm <= GARAGE_PARKING_OCCUPIED_CM)
      observed = 1;
    else if (sample.distanceCm >= GARAGE_PARKING_VACANT_CM)
      observed = 0;
  }
  if (observed < 0 || observed == current) {
    count = 0;
    return;
  }
  if (observed != candidate) {
    candidate = observed;
    count = 0;
  }
  if (++count < GARAGE_PARKING_STABLE_SAMPLES)
    return;
  count = 0;
  Event event{};
  esp_fill_random(event.nonce, sizeof(event.nonce));
  event.occurredMs = utcMs();
  event.occupied = observed == 1;
  if (xQueueSend(events, &event, 0) == pdTRUE)
    current = observed;
  else
    ESP_LOGW(tag, "Parking queue full; will recheck stable occupancy");
}
}

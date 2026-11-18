#include "network.h"
#include "config.h"
#include "protocol.h"
#include "radio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <atomic>
#include <cstdio>
#include <cstring>
namespace garage::network {
static QueueHandle_t queue_;
static esp_mqtt_client_handle_t client = nullptr;
static std::atomic<bool> hasIp{false}, mqttReady{false};
static bool configured = false, started = false;
static int64_t nextWifi = 0;
static char statusTopic[192], commandTopic[192], willPayload[384];
void topic(char *out, size_t length, const char *suffix) {
  snprintf(out, length, "%s/%s/%s", kTopicPrefix, GARAGE_DEVICE_ID, suffix);
}
static void wifiEvent(void *, esp_event_base_t base, int32_t id, void *data) {
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    hasIp = true;
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    hasIp = false;
    const auto *event = static_cast<wifi_event_sta_disconnected_t *>(data);
    ESP_LOGW("network", "Wi-Fi disconnected reason=%u; retry without credential logging", unsigned(event->reason));
  }
}
static void mqttEvent(void *, esp_event_base_t, int32_t id, void *data) {
  auto *e = static_cast<esp_mqtt_event_handle_t>(data);
  NetworkMessage msg{};
  if (id == MQTT_EVENT_CONNECTED) {
    mqttReady = false;
    esp_mqtt_client_subscribe(client, commandTopic, 0);
    return;
  }
  if (id == MQTT_EVENT_SUBSCRIBED) {
    if (e->data_len > 0 && static_cast<unsigned char>(e->data[0]) == 0x80) {
      ESP_LOGE("network", "Command subscription denied");
      return;
    }
    mqttReady = true;
    msg.kind = NetworkMessage::Connected;
  } else if (id == MQTT_EVENT_DISCONNECTED) {
    mqttReady = false;
    msg.kind = NetworkMessage::Disconnected;
  } else if (id == MQTT_EVENT_DATA) {
    // Reject fragmented/oversized frames instead of parsing a partial command.
    if (e->current_data_offset != 0 || e->total_data_len != e->data_len || e->data_len <= 0 ||
        e->data_len > 1024)
      return;
    if (e->topic_len != int(strlen(commandTopic)) || memcmp(e->topic, commandTopic, e->topic_len))
      return;
    msg.kind = NetworkMessage::CommandData;
    msg.retained = e->retain;
    msg.length = e->data_len;
    memcpy(msg.payload, e->data, msg.length);
    msg.payload[msg.length] = 0;
  } else if (id == MQTT_EVENT_ERROR) {
    ESP_LOGW("network", "MQTT error (credentials and certificate are not logged)");
    return;
  } else
    return;
  if (xQueueSend(queue_, &msg, 0) != pdTRUE)
    ESP_LOGW("network", "Queue full: dropped network message, no command replay");
}
void begin(QueueHandle_t queue) {
  queue_ = queue;
  if (!GARAGE_NETWORK_ENABLED) {
    ESP_LOGI("network", "Bring-up only: Wi-Fi/MQTT disabled");
    return;
  }
  if (!strlen(GARAGE_WIFI_SSID)) {
    ESP_LOGW("network", "Wi-Fi not configured: set SSID in config.local.h");
    return;
  }
  if (strlen(GARAGE_DEVICE_ID) > 48 || !strlen(GARAGE_DEVICE_ID) ||
      strspn(GARAGE_DEVICE_ID,
             "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") !=
          strlen(GARAGE_DEVICE_ID)) {
    ESP_LOGE("network", "Invalid deviceId");
    return;
  }
  // Wi-Fi driver may already be running for ESP-NOW (node_link starts first); start() is idempotent.
  radio::start();
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEvent, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifiEvent, nullptr));
  wifi_config_t wifi{};
  strncpy(reinterpret_cast<char *>(wifi.sta.ssid), GARAGE_WIFI_SSID, sizeof(wifi.sta.ssid));
  strncpy(reinterpret_cast<char *>(wifi.sta.password), GARAGE_WIFI_PASSWORD,
          sizeof(wifi.sta.password));
  wifi.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi)); // connect happens in poll()
  esp_sntp_config_t ntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(GARAGE_SNTP_SERVER);
  ntp.sync_cb = [](timeval *) { markTimeSynchronized(); };
  ESP_ERROR_CHECK(esp_netif_sntp_init(&ntp));
  configured = true;
  const bool localTransport = GARAGE_LOCAL_TEST && GARAGE_SIMULATED_HARDWARE &&
      !strncmp(GARAGE_MQTT_URI, "mqtt://", 7);
  if ((!localTransport && strncmp(GARAGE_MQTT_URI, "mqtts://", 8)) || strchr(GARAGE_MQTT_URI, '@') ||
      !strlen(GARAGE_MQTT_USERNAME) || !strlen(GARAGE_MQTT_PASSWORD)) {
    ESP_LOGW(
        "network",
        "Wi-Fi test only: MQTT needs mqtts URI and per-device credentials; no anonymous broker");
    return;
  }
  topic(statusTopic, sizeof(statusTopic), "status");
  topic(commandTopic, sizeof(commandTopic), "command");
  snprintf(willPayload, sizeof(willPayload),
           "{\"schemaVersion\":%d,\"deviceId\":\"%s\",\"timestamp\":null,\"timeSynced\":false,"
           "\"online\":false,\"reason\":\"last_will\"}",
           kSchemaVersion, GARAGE_DEVICE_ID);
  esp_mqtt_client_config_t mqtt{};
  mqtt.broker.address.uri = GARAGE_MQTT_URI;
  if (localTransport)
    ESP_LOGW("network", "LOCAL TEST ONLY: unencrypted MQTT; actuator disabled");
  else if (strlen(GARAGE_MQTT_CA_PEM))
    mqtt.broker.verification.certificate = GARAGE_MQTT_CA_PEM;
  else
    mqtt.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
  mqtt.credentials.client_id = GARAGE_DEVICE_ID;
  mqtt.credentials.username = GARAGE_MQTT_USERNAME;
  mqtt.credentials.authentication.password = GARAGE_MQTT_PASSWORD;
  mqtt.session.disable_clean_session = false;
  mqtt.session.keepalive = 10;
  mqtt.session.last_will.topic = statusTopic;
  mqtt.session.last_will.msg = willPayload;
  mqtt.session.last_will.qos = 0;
  mqtt.session.last_will.retain = true;
  mqtt.network.reconnect_timeout_ms = 3000;
  mqtt.buffer.size = 2048;
  client = esp_mqtt_client_init(&mqtt);
  if (!client) {
    ESP_LOGE("network", "MQTT init failed");
    return;
  }
  ESP_ERROR_CHECK(esp_mqtt_client_register_event(
      client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqttEvent, nullptr));
  configured = true;
}
bool wifiConnected() { return hasIp; }
bool mqttConnected() { return hasIp && mqttReady; }
void poll(int64_t now) {
  if (!configured)
    return;
  if (!hasIp && now >= nextWifi) {
    nextWifi = now + 5000;
    esp_wifi_connect();
  }
  // Never disable TLS verification to work around an unsynchronised clock.
  if (client && hasIp && clockSynced() && !started) {
    started = true;
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
  }
}
bool publish(const char *suffix, const char *json, bool retain) {
  if (!client || !started || !mqttConnected())
    return false;
  char dest[192];
  topic(dest, sizeof(dest), suffix);
  // enqueue(qos=0, store=false) discards the packet in ESP-MQTT. Publish
  // directly while connected, without keeping old telemetry/acks for replay.
  return esp_mqtt_client_publish(client, dest, json, 0, 0, retain) >= 0;
}
} // namespace garage::network

#include "node_link.h"
#include "contract.generated.h"
#include "garage_protocol.h"
#include "pins.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstring>
namespace garage::node_link {
namespace {
constexpr const char *TAG = "node_link";
constexpr uart_port_t kPort = UART_NUM_1; // UART0 is the USB console
constexpr int kBaud = 115200;
constexpr int kRxBufferBytes = 1024;
constexpr int kStatsPeriodMs = 10000;

// Latest data per message type. Written by the rx task, read by the main loop -> guarded by a mutex.
struct Latest {
  bool hasUltrasonic = false;
  gp_ultrasonic_t ultrasonic{};
  int64_t ultrasonicMs = 0;
  bool hasReadings = false;
  gp_readings_t readings{};
  int64_t readingsMs = 0;
};
Latest latest;
SemaphoreHandle_t latestLock = nullptr;

// Only touched by the rx task.
struct Stats {
  uint32_t bytes = 0, frames = 0, dups = 0, lost = 0, bad = 0;
} stats;
int32_t lastSeq[GP_MAX_NODES];

int64_t nowMs() { return esp_timer_get_time() / 1000; }

void sendAck(uint8_t nodeId, uint16_t seq) {
  uint8_t f[GP_MAX_FRAME];
  const size_t n = gp_build_frame(f, GP_MSG_ACK, nodeId, seq, nullptr, 0);
  uart_write_bytes(kPort, f, n);
}

bool knownFrame(uint8_t type, uint8_t len) {
  return (type == GP_MSG_ULTRASONIC && len == sizeof(gp_ultrasonic_t)) ||
         (type == GP_MSG_READINGS && len == sizeof(gp_readings_t));
}

void handleFrame(const uint8_t *f) {
  const uint8_t id = gp_node_id(f), type = gp_type(f), len = gp_len(f);
  const uint16_t seq = gp_seq(f);
  if (!knownFrame(type, len) || id >= GP_MAX_NODES) {
    stats.bad++;
    ESP_LOGW(TAG, "unexpected frame type=0x%02X len=%u node=%u", type, len, id);
    return;
  }

  sendAck(id, seq); // ACK duplicates too: the first ACK may have been lost
  if (lastSeq[id] == seq) {
    stats.dups++;
    ESP_LOGI(TAG, "dup node=%u seq=%u", id, seq);
    return;
  }
  // A gap in SEQ = lost frames. seq==0 after a higher seq means the node rebooted, not loss.
  if (lastSeq[id] >= 0 && seq != 0) {
    const uint16_t gap = uint16_t(seq - uint16_t(lastSeq[id]) - 1);
    if (gap) {
      stats.lost += gap;
      ESP_LOGW(TAG, "lost %u frame(s) before node=%u seq=%u", gap, id, seq);
    }
  }
  lastSeq[id] = seq;
  stats.frames++;

  // memcpy: the payload inside the frame buffer is not aligned.
  xSemaphoreTake(latestLock, portMAX_DELAY);
  if (type == GP_MSG_ULTRASONIC) {
    memcpy(&latest.ultrasonic, gp_payload(f), sizeof(latest.ultrasonic));
    latest.ultrasonicMs = nowMs();
    latest.hasUltrasonic = true;
  } else {
    memcpy(&latest.readings, gp_payload(f), sizeof(latest.readings));
    latest.readingsMs = nowMs();
    latest.hasReadings = true;
  }
  const Latest copy = latest;
  xSemaphoreGive(latestLock);

  if (type == GP_MSG_ULTRASONIC) {
    if (copy.ultrasonic.status == GP_US_OK)
      ESP_LOGI(TAG, "node=%u seq=%u dist=%u mm", id, seq, copy.ultrasonic.distance_mm);
    else
      ESP_LOGI(TAG, "node=%u seq=%u dist=NO_ECHO", id, seq);
  } else {
    const auto &r = copy.readings;
    ESP_LOGI(TAG, "node=%u seq=%u T=%.2f H=%.2f ldr=%u eco2=%u tvoc=%u aqi=%u led=%u", id, seq,
             r.temp_c_x100 / 100.0, r.humidity_x100 / 100.0, r.ldr_raw, r.eco2_ppm, r.tvoc_ppb,
             r.aqi, r.led_on);
  }
}

void logStats() {
  const uint32_t expected = stats.frames + stats.lost;
  ESP_LOGI(TAG, "stats: bytes=%lu frames=%lu dup=%lu lost=%lu bad=%lu delivery=%.1f%%",
           (unsigned long)stats.bytes, (unsigned long)stats.frames, (unsigned long)stats.dups,
           (unsigned long)stats.lost, (unsigned long)stats.bad,
           expected ? 100.0 * stats.frames / expected : 0.0);
  // Wiring hints for bring-up.
  if (stats.bytes == 0)
    ESP_LOGW(TAG, "no bytes received: check node TX -> GPIO%d, common GND, node powered", pins::UartRx);
  else if (stats.frames == 0)
    ESP_LOGW(TAG, "bytes but no valid frames: check baud (%d) on both sides", kBaud);
}

void rxTask(void *) {
  gp_parser_t parser{};
  uint8_t rx[64];
  int64_t lastStats = nowMs();
  for (;;) {
    const int n = uart_read_bytes(kPort, rx, sizeof(rx), pdMS_TO_TICKS(50));
    for (int i = 0; i < n; i++) {
      stats.bytes++;
      if (gp_parser_feed(&parser, rx[i]))
        handleFrame(parser.buf);
    }
    if (nowMs() - lastStats >= kStatsPeriodMs) {
      lastStats = nowMs();
      logStats();
    }
  }
}
} // namespace

bool begin() {
  if (pins::UartTx < 0 || pins::UartRx < 0) {
    ESP_LOGW(TAG, "UART pins not assigned in pins.h: node link disabled");
    return false;
  }
  for (auto &s : lastSeq)
    s = -1;
  latestLock = xSemaphoreCreateMutex();

  uart_config_t cfg{};
  cfg.baud_rate = kBaud;
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;
  ESP_ERROR_CHECK(uart_driver_install(kPort, kRxBufferBytes, 0, 0, nullptr, 0));
  ESP_ERROR_CHECK(uart_param_config(kPort, &cfg));
  ESP_ERROR_CHECK(uart_set_pin(kPort, pins::UartTx, pins::UartRx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  // Priority 4: below door/sensor safety tasks, above cloud upload (see docs/protocol.md).
  xTaskCreate(rxTask, "node_link_rx", 4096, nullptr, 4, nullptr);
  ESP_LOGI(TAG, "listening on UART1 TX=%d RX=%d @ %d", pins::UartTx, pins::UartRx, kBaud);
  return true;
}

void fill(SensorSnapshot &s, int64_t monotonicMs) {
  if (!latestLock)
    return;
  xSemaphoreTake(latestLock, portMAX_DELAY);
  const Latest copy = latest;
  xSemaphoreGive(latestLock);

  if (copy.hasUltrasonic && monotonicMs - copy.ultrasonicMs <= kStaleMs) {
    s.ultrasonicSampleMs = copy.ultrasonicMs;
    if (copy.ultrasonic.status == GP_US_OK)
      s.distanceCm = copy.ultrasonic.distance_mm / 10.0;
  }

  if (copy.hasReadings && monotonicMs - copy.readingsMs <= kStaleMs) {
    const auto &r = copy.readings;
    s.temperature = r.temp_c_x100 / 100.0;
    s.humidity = r.humidity_x100 / 100.0;
    s.ldrRaw = r.ldr_raw;
    s.ledOn = r.led_on;
    // ENS160 is still a stub on the node and sends 0 = "no reading".
    if (r.aqi) {
      s.aqi = r.aqi;
      s.tvocPpb = r.tvoc_ppb;
      s.eco2Ppm = r.eco2_ppm;
    }
  }
}
} // namespace garage::node_link

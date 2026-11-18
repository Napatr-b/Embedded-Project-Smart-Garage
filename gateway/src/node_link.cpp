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
#include <cstdio>
#include <cstring>
// Transport: 1 = ESP-NOW radio (default), 0 = wired UART1. Same frames, ACK and dedupe either way.
#ifndef GARAGE_LINK_ESPNOW
#define GARAGE_LINK_ESPNOW 1
#endif
// Channel used when the gateway is NOT joined to a router (bring-up). With a router, the radio
// must follow the router's channel and the node hops channels until it finds the gateway.
#ifndef GARAGE_ESPNOW_CHANNEL
#define GARAGE_ESPNOW_CHANNEL 1
#endif
#if GARAGE_LINK_ESPNOW
#include "config.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "radio.h"
#endif
#ifndef GARAGE_UART_LOOPBACK_TEST
#define GARAGE_UART_LOOPBACK_TEST 0 // 1 only in env gateway_uart_loopback
#endif
#ifndef GARAGE_UART_PIN_FINDER
#define GARAGE_UART_PIN_FINDER 0 // 1 only in env gateway_pin_finder
#endif
#if GARAGE_UART_PIN_FINDER
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include "esp_rom_sys.h"
#endif
namespace garage::node_link {
namespace {
constexpr const char *TAG = "node_link";
constexpr uart_port_t kPort = UART_NUM_1; // UART0 is the USB console
constexpr int kBaud = 115200;
constexpr int kRxBufferBytes = 1024;
constexpr int kStatsPeriodMs = 10000;
// Node counts as OFFLINE after this long without a valid frame. Above the slowest send period
// (READINGS every 5 s) so a node with only env sensors enabled doesn't flap.
constexpr int64_t kOfflineMs = 6000;

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
  uint32_t bytesAtLastLog = 0; // to show bytes received in the last period, not only the total
} stats;
// Last raw bytes received, printed when bytes arrive but nothing decodes. Readable text here means
// the wire is on the node's console pin (TX0); random values mean noise or a wrong baud rate.
uint8_t rawTail[16];
uint32_t rawTailCount = 0;
int32_t lastSeq[GP_MAX_NODES];
int64_t lastFrameMs[GP_MAX_NODES]; // when each node's last valid frame arrived
bool online[GP_MAX_NODES];

int64_t nowMs() { return esp_timer_get_time() / 1000; }

#if GARAGE_LINK_ESPNOW
// One received ESP-NOW packet. The receive callback runs in the Wi-Fi task, so it only copies the
// packet into a queue; rxTask does the parsing and ACKing (deferred processing, like an ISR).
struct RxPacket {
  uint8_t mac[6];
  uint8_t len;
  uint8_t data[GP_MAX_FRAME];
};
QueueHandle_t rxQueue = nullptr;
uint8_t senderMac[6]; // MAC of the packet rxTask is handling now: the ACK goes back to it
uint32_t queueDrops = 0;

void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len <= 0 || len > GP_MAX_FRAME)
    return;
  RxPacket pkt{};
  memcpy(pkt.mac, info->src_addr, 6);
  pkt.len = uint8_t(len);
  memcpy(pkt.data, data, len);
  if (xQueueSend(rxQueue, &pkt, 0) != pdTRUE)
    queueDrops++; // the node will retry, so a drop costs one retry, not data
}

void addPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac))
    return;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0; // 0 = current radio channel
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false; // TODO(E5): PMK/LMK from config.local.h
  if (esp_now_add_peer(&peer) == ESP_OK)
    ESP_LOGI(TAG, "ESP-NOW peer added %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
}
#endif

void sendAck(uint8_t nodeId, uint16_t seq) {
  uint8_t f[GP_MAX_FRAME];
  const size_t n = gp_build_frame(f, GP_MSG_ACK, nodeId, seq, nullptr, 0);
#if GARAGE_LINK_ESPNOW
  addPeer(senderMac); // unicast needs a registered peer; first frame from a node registers it
  esp_now_send(senderMac, f, n);
#else
  uart_write_bytes(kPort, f, n);
#endif
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
  lastFrameMs[id] = nowMs();
  if (!online[id]) {
    online[id] = true;
    ESP_LOGI(TAG, "node=%u ONLINE (first valid frame seq=%u)", id, seq);
  }
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

// Edge-triggered: logs once when a node goes quiet, once when it comes back (see handleFrame).
void checkOffline() {
  const int64_t now = nowMs();
  for (int id = 0; id < GP_MAX_NODES; id++) {
    if (online[id] && now - lastFrameMs[id] > kOfflineMs) {
      online[id] = false;
      ESP_LOGW(TAG, "node=%d OFFLINE (no valid frame for %.1f s)", id, (now - lastFrameMs[id]) / 1000.0);
    }
  }
}

void logStats() {
  const uint32_t expected = stats.frames + stats.lost;
  const uint32_t recent = stats.bytes - stats.bytesAtLastLog;
  stats.bytesAtLastLog = stats.bytes;
  // bytes_10s shows whether data is flowing *now*; the total alone can't tell you it stopped.
  ESP_LOGI(TAG, "stats: bytes_10s=%lu bytes=%lu frames=%lu dup=%lu lost=%lu bad=%lu delivery=%.1f%%",
           (unsigned long)recent, (unsigned long)stats.bytes, (unsigned long)stats.frames,
           (unsigned long)stats.dups, (unsigned long)stats.lost, (unsigned long)stats.bad,
           expected ? 100.0 * stats.frames / expected : 0.0);
  for (int id = 0; id < GP_MAX_NODES; id++)
    if (lastSeq[id] >= 0)
      ESP_LOGI(TAG, "  node=%d %s, last frame %.1f s ago, last seq=%ld", id,
               online[id] ? "online" : "OFFLINE", (nowMs() - lastFrameMs[id]) / 1000.0, (long)lastSeq[id]);
  // Hints for bring-up.
#if GARAGE_LINK_ESPNOW
  uint8_t primary = 0;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  if (queueDrops)
    ESP_LOGW(TAG, "ESP-NOW rx queue dropped %lu packet(s)", (unsigned long)queueDrops);
  if (stats.bytes == 0)
    ESP_LOGW(TAG, "no ESP-NOW packets yet (gateway on channel %u): node powered? LINK_USE_ESPNOW=1? "
                  "node hops channels after a few misses, give it up to ~1 min", primary);
#else
  if (stats.bytes == 0)
    ESP_LOGW(TAG, "no bytes received: check node TX -> GPIO%d, common GND, node powered", pins::UartRx);
#endif
  else if (stats.frames == 0) {
    ESP_LOGW(TAG, "bytes but no valid frames: check baud (%d) on both sides", kBaud);
    char hex[sizeof(rawTail) * 3 + 1] = "", text[sizeof(rawTail) + 1] = "";
    const uint32_t n = rawTailCount < sizeof(rawTail) ? rawTailCount : sizeof(rawTail);
    for (uint32_t i = 0; i < n; i++) {
      const uint8_t b = rawTail[(rawTailCount - n + i) % sizeof(rawTail)];
      snprintf(hex + i * 3, 4, "%02X ", b);
      text[i] = (b >= 32 && b < 127) ? char(b) : '.';
    }
    ESP_LOGW(TAG, "last %lu raw bytes: %s |%s|", (unsigned long)n, hex, text);
  }
}

void rxTask(void *) {
  gp_parser_t parser{};
  uint8_t rx[64];
  int64_t lastStats = nowMs();
  for (;;) {
#if GARAGE_LINK_ESPNOW
    // Each packet holds one whole frame; reset the parser so a bad packet can't spill into the next.
    RxPacket pkt;
    int n = 0;
    if (xQueueReceive(rxQueue, &pkt, pdMS_TO_TICKS(50)) == pdTRUE) {
      memcpy(senderMac, pkt.mac, 6);
      memcpy(rx, pkt.data, pkt.len);
      n = pkt.len;
      parser = gp_parser_t{};
    }
#else
    const int n = uart_read_bytes(kPort, rx, sizeof(rx), pdMS_TO_TICKS(50));
#endif
    for (int i = 0; i < n; i++) {
      stats.bytes++;
      rawTail[rawTailCount++ % sizeof(rawTail)] = rx[i];
      if (gp_parser_feed(&parser, rx[i]))
        handleFrame(parser.buf);
    }
    checkOffline();
    if (nowMs() - lastStats >= kStatsPeriodMs) {
      lastStats = nowMs();
      logStats();
    }
  }
}
#if GARAGE_UART_LOOPBACK_TEST
// UART self-test (env gateway_uart_loopback): jumper GPIO17 (TX) -> GPIO18 (RX) on the gateway,
// node unplugged. Every frame we send must come back byte for byte with a valid CRC.
// Passing here + failing with the node = the problem is the wires between boards or the node.
void loopbackTask(void *) {
  constexpr int kFrames = 20;
  for (;;) {
    int ok = 0;
    uint32_t bytesBack = 0;
    for (int i = 0; i < kFrames; i++) {
      uint8_t frame[GP_MAX_FRAME];
      const gp_ultrasonic_t m{uint16_t(100 + i), GP_US_OK};
      const size_t n = gp_build_frame(frame, GP_MSG_ULTRASONIC, 0, uint16_t(i), &m, sizeof(m));
      uart_flush_input(kPort);
      uart_write_bytes(kPort, frame, n);

      gp_parser_t parser{};
      uint8_t rx[GP_MAX_FRAME];
      bool match = false;
      const int64_t start = nowMs();
      while (!match && nowMs() - start < 50) {
        const int got = uart_read_bytes(kPort, rx, sizeof(rx), pdMS_TO_TICKS(10));
        for (int j = 0; j < got && !match; j++) {
          bytesBack++;
          match = gp_parser_feed(&parser, rx[j]) == n && memcmp(parser.buf, frame, n) == 0;
        }
      }
      ok += match;
    }
    if (ok == kFrames)
      ESP_LOGI(TAG, "[loopback] %d/%d frames echoed, %lu bytes back: PASS, gateway UART pins work", ok,
               kFrames, (unsigned long)bytesBack);
    else if (bytesBack == 0)
      ESP_LOGE(TAG, "[loopback] 0/%d: FAIL, nothing back. Jumper GPIO%d -> GPIO%d missing, or pin dead",
               kFrames, pins::UartTx, pins::UartRx);
    else
      ESP_LOGE(TAG, "[loopback] %d/%d, %lu bytes back: FAIL, corrupted. Loose jumper or noise", ok, kFrames,
               (unsigned long)bytesBack);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
#endif
#if GARAGE_UART_PIN_FINDER
// Pin finder (env gateway_pin_finder): "which gateway pin is wire A really on?"
// Every free GPIO becomes an input with a pull-down. The node keeps sending (with retries), so the pin
// its TX2 wire lands on shows edges (data) and otherwise sits HIGH (UART idle level). A pin with no
// wire reads a steady LOW because of the pull-down.
// Excluded: 19/20 (USB), 26-37 (flash + octal PSRAM), 43/44 (console UART0).
constexpr int kFinderPins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                               21, 38, 39, 40, 41, 42, 45, 46, 47, 48};

uint64_t readAllPins() {
  return uint64_t(REG_READ(GPIO_IN_REG)) | (uint64_t(REG_READ(GPIO_IN1_REG) & 0x3FFFFF) << 32);
}

// Self-check: flip each pin's internal pull-up / pull-down and read it back.
//  - follows the pull both ways -> nothing connected (and proves the register read works)
//  - stays HIGH with pull-down   -> something drives it HIGH, e.g. an idle UART TX wire
//  - stays LOW with pull-up      -> something drives it LOW (GND, or a powered-off board's pin)
void pullCheck(uint64_t mask) {
  int followed = 0;
  for (int p : kFinderPins) {
    gpio_set_pull_mode(gpio_num_t(p), GPIO_PULLUP_ONLY);
    esp_rom_delay_us(200);
    const bool upReads = readAllPins() >> p & 1;
    gpio_set_pull_mode(gpio_num_t(p), GPIO_PULLDOWN_ONLY);
    esp_rom_delay_us(200);
    const bool downReads = readAllPins() >> p & 1;
    if (upReads && !downReads)
      followed++;
    else if (upReads && downReads)
      ESP_LOGW(TAG, "[pin finder] GPIO%-2d is DRIVEN HIGH by something outside (idle TX line?)", p);
    else if (!upReads && !downReads)
      ESP_LOGW(TAG, "[pin finder] GPIO%-2d is DRIVEN LOW by something outside (GND? unpowered board?)", p);
    else
      ESP_LOGW(TAG, "[pin finder] GPIO%-2d reads inverted (?)", p);
  }
  ESP_LOGI(TAG, "[pin finder] pull check: %d/%d pins follow the internal pull = unconnected%s", followed,
           int(sizeof(kFinderPins) / sizeof(kFinderPins[0])),
           followed ? " (pin reading works)" : " -- NO pin follows, reading may be broken");
  (void)mask;
}

void pinFinderTask(void *) {
  uint64_t mask = 0;
  for (int p : kFinderPins) {
    gpio_reset_pin(gpio_num_t(p));
    gpio_set_direction(gpio_num_t(p), GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio_num_t(p), GPIO_PULLDOWN_ONLY);
    mask |= 1ULL << p;
  }
  vTaskDelay(pdMS_TO_TICKS(50));
  for (;;) {
    // Busy-poll for 900 ms (a bit at 115200 baud is 8.7 us, so a slow poll would miss edges),
    // then sleep 1 s so the idle task and watchdog get CPU time.
    uint32_t edges[49] = {};
    uint64_t prev = readAllPins() & mask, everHigh = 0, everLow = 0;
    const int64_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < 900000) {
      const uint64_t now = readAllPins() & mask;
      everHigh |= now;
      everLow |= ~now & mask;
      for (uint64_t changed = now ^ prev; changed; changed &= changed - 1)
        edges[__builtin_ctzll(changed)]++;
      prev = now;
    }
    bool dataOnRx = false, anyData = false;
    ESP_LOGI(TAG, "[pin finder] ---- scan (node must be running) ----");
    pullCheck(mask);
    for (int p : kFinderPins) {
      const bool high = everHigh >> p & 1, low = everLow >> p & 1;
      if (edges[p]) {
        anyData = true;
        dataOnRx |= p == pins::UartRx;
        ESP_LOGW(TAG, "[pin finder] GPIO%-2d %5lu edges  <- DATA%s", p, (unsigned long)edges[p],
                 p == pins::UartRx ? " (correct RX pin)" : "");
      } else if (high && !low) {
        ESP_LOGI(TAG, "[pin finder] GPIO%-2d steady HIGH (something drives it: idle TX line, 3V3, or a pull-up)", p);
      }
    }
    if (dataOnRx)
      ESP_LOGI(TAG, "[pin finder] wire A is on GPIO%d as expected. Flash gateway_bringup again.", pins::UartRx);
    else if (anyData)
      ESP_LOGW(TAG, "[pin finder] node data is on the pin marked DATA, not GPIO%d: move wire A to GPIO%d",
               pins::UartRx, pins::UartRx);
    else
      ESP_LOGE(TAG, "[pin finder] no data on any free gateway pin: wire A doesn't reach the gateway, or it "
                    "isn't on the node's TX2 (GPIO17). Check the node end, then run the node loopback test.");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
#endif
} // namespace

bool begin() {
#if GARAGE_UART_PIN_FINDER
  ESP_LOGW(TAG, "UART PIN FINDER MODE: node link disabled, scanning free GPIOs for node data");
  xTaskCreate(pinFinderTask, "pin_finder", 4096, nullptr, 4, nullptr);
  return true;
#endif
  for (auto &s : lastSeq)
    s = -1;
  latestLock = xSemaphoreCreateMutex();

#if GARAGE_LINK_ESPNOW && !GARAGE_UART_LOOPBACK_TEST
  rxQueue = xQueueCreate(16, sizeof(RxPacket));
  radio::start();
  if (!GARAGE_NETWORK_ENABLED) // no router: pick a fixed channel the node starts on
    ESP_ERROR_CHECK(esp_wifi_set_channel(GARAGE_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_recv_cb(onEspNowRecv));
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  xTaskCreate(rxTask, "node_link_rx", 4096, nullptr, 4, nullptr);
  ESP_LOGI(TAG, "listening on ESP-NOW, channel %d, gateway MAC %02X:%02X:%02X:%02X:%02X:%02X",
           GARAGE_NETWORK_ENABLED ? -1 : GARAGE_ESPNOW_CHANNEL, mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
  return true;
#endif

  if (pins::UartTx < 0 || pins::UartRx < 0) {
    ESP_LOGW(TAG, "UART pins not assigned in pins.h: node link disabled");
    return false;
  }

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

#if GARAGE_UART_LOOPBACK_TEST
  ESP_LOGW(TAG, "UART LOOPBACK TEST MODE: node link disabled, repeating self-test every 2 s");
  xTaskCreate(loopbackTask, "uart_loopback", 4096, nullptr, 4, nullptr);
  return true;
#endif
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

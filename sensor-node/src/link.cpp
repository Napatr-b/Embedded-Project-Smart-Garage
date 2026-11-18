#include "link.h"
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "garage_protocol.h"
#include "pins.h"
#if LINK_USE_ESPNOW
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#endif

/*
 * Two transports carry the SAME garage_protocol.h frames (node ID, SEQ, CRC16):
 *   LINK_USE_ESPNOW 1  ESP-NOW radio (default): no wires between the boards.
 *   LINK_USE_ESPNOW 0  wired UART2 (GPIO16/17), the original design.
 * ACK/retry, SEQ and dedupe work the same either way; only "send a frame" and
 * "wait for the ACK" below differ.
 */

static HardwareSerial Link(2);      /* UART transport, also used by the loopback self-test */
static uint16_t seq = 0;            /* one counter for all message types: gateway counts gaps as loss */
static LinkStats stats = {0, 0};
static bool led_on = false;
static uint32_t led_on_since = 0;

#if LINK_USE_ESPNOW
static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t gateway_mac[6];
static bool gateway_known = false;   /* learned from the first ACK; until then we broadcast */
static uint8_t channel = ESPNOW_FIRST_CHANNEL;
static int misses_on_channel = 0;    /* messages with no ACK on the current channel */

/* Written by the Wi-Fi task (receive callback), read by loop(). One 32-bit word is written
 * atomically on the ESP32, so no lock is needed: bit 16 = "an ACK arrived", low 16 bits = its SEQ. */
static volatile uint32_t ack_word = 0;
static volatile bool mac_pending = false;
static uint8_t pending_mac[6];

/* Runs in the Wi-Fi task: keep it short. Validates the frame (CRC) and records the ACK. */
static void on_espnow_recv(const uint8_t *mac, const uint8_t *data, int len) {
  gp_parser_t p = {};
  for (int i = 0; i < len; i++) {
    if (gp_parser_feed(&p, data[i]) && gp_type(p.buf) == GP_MSG_ACK && gp_node_id(p.buf) == NODE_ID) {
      if (!gateway_known && !mac_pending) {
        memcpy(pending_mac, mac, 6);
        mac_pending = true;
      }
      ack_word = (1u << 16) | gp_seq(p.buf);
    }
  }
}

static void add_peer(const uint8_t *mac) {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;                  /* 0 = whatever channel the radio is on now */
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;              /* TODO(E5): set a PMK/LMK from secrets.h */
  if (!esp_now_is_peer_exist(mac)) esp_now_add_peer(&peer);
}

static void set_channel(uint8_t ch) {
  channel = ch;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

static void espnow_begin() {
  WiFi.mode(WIFI_STA);               /* radio on, but we never join a network */
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("[link] esp_now_init FAILED");
    return;
  }
  esp_now_register_recv_cb(on_espnow_recv);
  set_channel(ESPNOW_FIRST_CHANNEL);
  add_peer(kBroadcast);
  Serial.printf("[link] ESP-NOW ready, my MAC %s, starting on channel %u\n", WiFi.macAddress().c_str(),
                channel);
}

/* The gateway sits on its Wi-Fi router's channel (or a fixed one with no router), which the node
 * can't know in advance. After a few unanswered messages we hop to the next channel; the first ACK
 * pins us there. */
static void espnow_after_send(bool acked) {
  if (acked) {
    misses_on_channel = 0;
    return;
  }
  if (++misses_on_channel < ESPNOW_MISSES_BEFORE_HOP) return;
  misses_on_channel = 0;
  gateway_known = false;             /* gateway may have moved: broadcast again until the next ACK */
  uint8_t next = channel >= 13 ? 1 : channel + 1;
  Serial.printf("[link] no gateway on channel %u, trying channel %u\n", channel, next);
  set_channel(next);
}
#endif

void link_begin() {
#if LINK_USE_ESPNOW
  espnow_begin();
#else
  Link.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
#endif
  if (PIN_LINK_LED >= 0) {
    pinMode(PIN_LINK_LED, OUTPUT);
    digitalWrite(PIN_LINK_LED, LOW);
  }
}

/* The LED only lights when an ACK came back, so a blink proves the whole round trip:
 * node -> gateway -> gateway parsed + CRC OK -> ACK -> node. No laptop needed. */
static void led_flash() {
  if (PIN_LINK_LED < 0) return;
  digitalWrite(PIN_LINK_LED, HIGH);
  led_on = true;
  led_on_since = millis();
}

void link_poll() {
  /* Non-blocking LED off, so the flash never delays sensor reads or the next send. */
  if (led_on && millis() - led_on_since >= LINK_LED_FLASH_MS) {
    digitalWrite(PIN_LINK_LED, LOW);
    led_on = false;
  }
}

static void tx_frame(const uint8_t *frame, size_t n) {
#if LINK_USE_ESPNOW
  esp_now_send(gateway_known ? gateway_mac : kBroadcast, frame, n);
#else
  Link.write(frame, n);
#endif
}

static bool wait_for_ack(uint16_t expect_seq) {
  uint32_t start = millis();
#if LINK_USE_ESPNOW
  while (millis() - start < ACK_TIMEOUT_MS) {
    uint32_t w = ack_word;
    if ((w >> 16) && (uint16_t)w == expect_seq) {
      if (mac_pending) {             /* first ACK: switch from broadcast to unicast */
        memcpy(gateway_mac, pending_mac, 6);
        add_peer(gateway_mac);
        gateway_known = true;
        mac_pending = false;
        Serial.printf("[link] gateway found on channel %u: %02X:%02X:%02X:%02X:%02X:%02X\n", channel,
                      gateway_mac[0], gateway_mac[1], gateway_mac[2], gateway_mac[3], gateway_mac[4],
                      gateway_mac[5]);
      }
      return true;
    }
    delay(1);
  }
#else
  gp_parser_t p = {};
  while (millis() - start < ACK_TIMEOUT_MS) {
    while (Link.available()) {
      size_t n = gp_parser_feed(&p, (uint8_t)Link.read());
      if (n && gp_type(p.buf) == GP_MSG_ACK &&
          gp_node_id(p.buf) == NODE_ID && gp_seq(p.buf) == expect_seq) {
        return true;
      }
    }
    delay(1);
  }
#endif
  return false;
}

bool link_send(uint8_t type, const void *payload, uint8_t len, int *tries) {
  uint8_t frame[GP_MAX_FRAME];
  size_t n = gp_build_frame(frame, type, NODE_ID, seq, payload, len);
  if (!n) return false;

#if LINK_USE_ESPNOW
  ack_word = 0;                            /* forget ACKs for older SEQs */
#else
  while (Link.available()) Link.read();   /* drop stale bytes (e.g. a late ACK for an old SEQ) */
#endif

  bool acked = false;
  int attempt = 0;
  /* The same SEQ is resent on retry, so the gateway can drop duplicates. */
  for (; attempt <= MAX_RETRIES && !acked; attempt++) {
    tx_frame(frame, n);
    acked = wait_for_ack(seq);
  }

  stats.sent++;
  if (acked) {
    stats.acked++;
    led_flash();
  }
#if LINK_USE_ESPNOW
  espnow_after_send(acked);
#endif
  if (tries) *tries = attempt;
  seq++;
  return acked;
}

uint16_t link_next_seq() { return seq; }

bool link_loopback_test() {
  /* UART only. With TX jumpered to RX, every byte we send comes straight back. We send real frames
   * and check they parse with a valid CRC and match byte for byte. This isolates the board: if this
   * passes but the real link fails, the problem is the wires between the boards or the gateway. */
  Link.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  const int kFrames = 20;
  int ok = 0;
  uint32_t bytes_back = 0;
  for (int i = 0; i < kFrames; i++) {
    gp_ultrasonic_t m = {(uint16_t)(100 + i), GP_US_OK};
    uint8_t frame[GP_MAX_FRAME];
    size_t n = gp_build_frame(frame, GP_MSG_ULTRASONIC, NODE_ID, (uint16_t)i, &m, sizeof(m));
    while (Link.available()) Link.read();
    Link.write(frame, n);

    gp_parser_t p = {};
    uint32_t start = millis();
    bool match = false;
    while (!match && millis() - start < 50) {
      while (Link.available()) {
        bytes_back++;
        size_t got = gp_parser_feed(&p, (uint8_t)Link.read());
        if (got == n && memcmp(p.buf, frame, n) == 0) match = true;
      }
      delay(1);
    }
    if (match) ok++;
  }

  Serial.printf("[loopback] %d/%d frames echoed OK, %lu bytes back: ", ok, kFrames,
                (unsigned long)bytes_back);
  if (ok == kFrames)
    Serial.println("PASS, this board's UART TX/RX pins work");
  else if (bytes_back == 0)
    Serial.printf("FAIL, nothing back: jumper GPIO%d -> GPIO%d missing, or pin dead\n", PIN_LINK_TX,
                  PIN_LINK_RX);
  else
    Serial.println("FAIL, bytes came back corrupted: loose jumper or noise");
  return ok == kFrames;
}

LinkStats link_stats() { return stats; }

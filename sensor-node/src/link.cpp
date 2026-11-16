#include "link.h"
#include <Arduino.h>
#include "config.h"
#include "garage_protocol.h"
#include "pins.h"

static HardwareSerial Link(2);
static uint16_t seq = 0;            /* one counter for all message types: gateway counts gaps as loss */
static LinkStats stats = {0, 0};

void link_begin() {
  Link.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
}

static bool wait_for_ack(uint16_t expect_seq) {
  gp_parser_t p = {};
  uint32_t start = millis();
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
  return false;
}

bool link_send(uint8_t type, const void *payload, uint8_t len, int *tries) {
  uint8_t frame[GP_MAX_FRAME];
  size_t n = gp_build_frame(frame, type, NODE_ID, seq, payload, len);
  if (!n) return false;

  while (Link.available()) Link.read();   /* drop stale bytes (e.g. a late ACK for an old SEQ) */

  bool acked = false;
  int attempt = 0;
  /* The same SEQ is resent on retry, so the gateway can drop duplicates. */
  for (; attempt <= MAX_RETRIES && !acked; attempt++) {
    Link.write(frame, n);
    acked = wait_for_ack(seq);
  }

  stats.sent++;
  if (acked) stats.acked++;
  if (tries) *tries = attempt;
  seq++;
  return acked;
}

uint16_t link_next_seq() { return seq; }

LinkStats link_stats() { return stats; }

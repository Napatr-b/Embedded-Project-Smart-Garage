#pragma once
/* UART link to the gateway: framed messages (garage_protocol.h) with ACK + retry. */
#include <stdint.h>
#include <stddef.h>

struct LinkStats {
  uint32_t sent;    /* messages we tried to deliver */
  uint32_t acked;   /* messages the gateway confirmed */
};

void link_begin();

/* Sends one message and waits for its ACK, retrying up to MAX_RETRIES times.
 * Blocks for at most (MAX_RETRIES + 1) * ACK_TIMEOUT_MS.
 * Returns true when ACKed; *tries (optional) gets the number of attempts used. */
bool link_send(uint8_t type, const void *payload, uint8_t len, int *tries = nullptr);

/* SEQ that the next link_send() will use. */
uint16_t link_next_seq();

LinkStats link_stats();

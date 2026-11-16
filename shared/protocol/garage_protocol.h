#pragma once
/*
 * Smart Garage: Node <-> Gateway UART protocol (shared by both firmwares).
 *
 * Frame (little-endian):
 *   [SOF 0xA5][VER][TYPE][NODE_ID][SEQ lo][SEQ hi][LEN][PAYLOAD x LEN][CRC lo][CRC hi]
 * CRC16-CCITT (poly 0x1021, init 0xFFFF) over VER .. end of PAYLOAD.
 *
 * Node sends GP_MSG_READINGS; gateway answers GP_MSG_ACK with the same NODE_ID + SEQ.
 * Node retries if no ACK arrives within the timeout (E4). The gateway drops
 * duplicates by remembering the last SEQ per NODE_ID.
 * See docs/protocol.md.
 */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP_SOF          0xA5
#define GP_VERSION      1
#define GP_MAX_PAYLOAD  32
#define GP_HEADER_LEN   7
#define GP_CRC_LEN      2
#define GP_MAX_FRAME    (GP_HEADER_LEN + GP_MAX_PAYLOAD + GP_CRC_LEN)
#define GP_MAX_NODES    8

typedef enum {
  GP_MSG_READINGS = 0x01,
  GP_MSG_ACK      = 0x80,
  GP_MSG_NACK     = 0x81,
} gp_msg_type_t;

typedef struct __attribute__((packed)) {
  int16_t  temp_c_x100;    /* DHT22 temperature  * 100 */
  uint16_t humidity_x100;  /* DHT22 humidity %   * 100 */
  uint16_t ldr_raw;        /* ADC raw 0..4095 */
  uint16_t eco2_ppm;       /* ENS160 */
  uint16_t tvoc_ppb;       /* ENS160 */
  uint8_t  aqi;            /* ENS160 AQI-UBA 1..5 */
  uint8_t  led_on;         /* lighting state */
} gp_readings_t;

static inline uint16_t gp_crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

/* Build a frame into out (size >= GP_MAX_FRAME). Returns frame length, 0 on error. */
static inline size_t gp_build_frame(uint8_t *out, uint8_t type, uint8_t node_id,
                                    uint16_t seq, const void *payload, uint8_t len) {
  if (len > GP_MAX_PAYLOAD) return 0;
  out[0] = GP_SOF;
  out[1] = GP_VERSION;
  out[2] = type;
  out[3] = node_id;
  out[4] = (uint8_t)(seq & 0xFF);
  out[5] = (uint8_t)(seq >> 8);
  out[6] = len;
  for (uint8_t i = 0; i < len; i++) out[GP_HEADER_LEN + i] = ((const uint8_t *)payload)[i];
  uint16_t crc = gp_crc16(&out[1], (size_t)(GP_HEADER_LEN - 1 + len));
  out[GP_HEADER_LEN + len]     = (uint8_t)(crc & 0xFF);
  out[GP_HEADER_LEN + len + 1] = (uint8_t)(crc >> 8);
  return (size_t)(GP_HEADER_LEN + len + GP_CRC_LEN);
}

/* Byte-by-byte stream parser. */
typedef struct {
  uint8_t buf[GP_MAX_FRAME];
  size_t  pos;
  size_t  expected;
} gp_parser_t;

/* Feed one byte. Returns frame length when p->buf holds a complete CRC-valid frame, else 0. */
static inline size_t gp_parser_feed(gp_parser_t *p, uint8_t byte) {
  if (p->pos == 0 && byte != GP_SOF) return 0;          /* wait for start of frame */
  p->buf[p->pos++] = byte;
  if (p->pos == GP_HEADER_LEN) {
    if (p->buf[1] != GP_VERSION || p->buf[6] > GP_MAX_PAYLOAD) { p->pos = 0; return 0; }
    p->expected = (size_t)GP_HEADER_LEN + p->buf[6] + GP_CRC_LEN;
  }
  if (p->pos >= GP_HEADER_LEN && p->pos == p->expected) {
    size_t n = p->pos;
    p->pos = 0;
    uint16_t rx = (uint16_t)(p->buf[n - 2] | (p->buf[n - 1] << 8));
    if (gp_crc16(&p->buf[1], n - 1 - GP_CRC_LEN) == rx) return n;
  }
  return 0;
}

static inline uint8_t  gp_type(const uint8_t *f)    { return f[2]; }
static inline uint8_t  gp_node_id(const uint8_t *f) { return f[3]; }
static inline uint16_t gp_seq(const uint8_t *f)     { return (uint16_t)(f[4] | (f[5] << 8)); }
static inline uint8_t  gp_len(const uint8_t *f)     { return f[6]; }
static inline const uint8_t *gp_payload(const uint8_t *f) { return &f[GP_HEADER_LEN]; }

#ifdef __cplusplus
}
#endif

#pragma once
/* Sensor node settings. Feature switches let us test one sensor at a time. */

#define NODE_ID              1      /* unique per node; carried in every frame (scalability) */

/* Link to the gateway (see docs/protocol.md). Same frames + ACK/retry on both transports. */
#define LINK_USE_ESPNOW      1      /* 1 = ESP-NOW radio (no wires), 0 = wired UART2 */
#define ESPNOW_FIRST_CHANNEL 1      /* gateway default with no router (GARAGE_ESPNOW_CHANNEL) */
#define ESPNOW_MISSES_BEFORE_HOP 3  /* unanswered messages before trying the next Wi-Fi channel */

/* UART transport settings */
#define LINK_BAUD            115200
#define ACK_TIMEOUT_MS       100    /* a ~20-byte frame takes <2 ms at 115200, so 100 ms is generous */
#define MAX_RETRIES          3      /* resend up to 3 more times when no ACK (E4) */
#define LINK_LED_FLASH_MS    40     /* PIN_LINK_LED flash length per ACK: steady 1 Hz blink = link OK */
#define LINK_LOOPBACK_TEST   0      /* 1 = boot into a UART self-test instead of normal mode.
                                       Unplug the gateway wires and put ONE jumper from GPIO17 to GPIO16
                                       on this board. Proves this board's UART pins work on their own. */

/* Features: 1 = read and send, 0 = skip */
#define FEATURE_LINK_TEST    0      /* 1 = send a fake test pattern every second, no sensors needed.
                                       Use it to test the link alone. Set back to 0 for real data. */
#define FEATURE_ULTRASONIC   1      /* HC-SR04 -> GP_MSG_ULTRASONIC */
#define FEATURE_ENV          0      /* DHT22 + LDR + ENS160 + LED -> GP_MSG_READINGS.
                                       Off until those sensors are wired, so the gateway
                                       doesn't log zeros as real readings. */

#define ULTRASONIC_PERIOD_MS 1000
#define ENV_PERIOD_MS        5000

#define LDR_DARK_THRESHOLD   1500   /* TODO: calibrate on the real model */

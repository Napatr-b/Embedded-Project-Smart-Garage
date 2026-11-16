#pragma once
/* Sensor node settings. Feature switches let us test one sensor at a time. */

#define NODE_ID              1      /* unique per node; carried in every frame (scalability) */

/* UART link to the gateway (see docs/protocol.md) */
#define LINK_BAUD            115200
#define ACK_TIMEOUT_MS       100    /* a ~20-byte frame takes <2 ms at 115200, so 100 ms is generous */
#define MAX_RETRIES          3      /* resend up to 3 more times when no ACK (E4) */

/* Features: 1 = read and send, 0 = skip */
#define FEATURE_ULTRASONIC   1      /* HC-SR04 -> GP_MSG_ULTRASONIC */
#define FEATURE_ENV          0      /* DHT22 + LDR + ENS160 + LED -> GP_MSG_READINGS.
                                       Off until those sensors are wired, so the gateway
                                       doesn't log zeros as real readings. */

#define ULTRASONIC_PERIOD_MS 1000
#define ENV_PERIOD_MS        5000

#define LDR_DARK_THRESHOLD   1500   /* TODO: calibrate on the real model */

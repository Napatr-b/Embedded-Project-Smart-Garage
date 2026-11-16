#pragma once
/*
 * Gateway pins: PROVISIONAL. Verify against docs/pin-map.md before wiring.
 * ESP32-S3-DevKitC-1-N8R8 reserved pins:
 *   GPIO35-37  octal PSRAM (do not use)
 *   GPIO19/20  USB D-/D+
 *   GPIO43/44  UART0 console (USB-UART bridge)
 *   GPIO0/3/45/46 strapping pins (avoid)
 * Camera (OV2640 DVP, ~16 signals) pins are TBD until the breakout is chosen;
 * re-check everything below for conflicts then.
 */
#define PIN_NODE_UART_TX    17
#define PIN_NODE_UART_RX    18

#define PIN_US_ENTRY_TRIG   4
#define PIN_US_ENTRY_ECHO   5
#define PIN_US_BAY_TRIG     6
#define PIN_US_BAY_ECHO     7

#define PIN_IR_OBSTACLE     15
#define PIN_SERVO           16

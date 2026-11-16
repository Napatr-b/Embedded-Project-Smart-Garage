#pragma once
/* Sensor node pins: PROVISIONAL, see docs/pin-map.md.
 * GPIO34-39 are input-only (good for LDR on ADC1). Avoid ADC2 pins: they can't be used while Wi-Fi is on (OTA/E6). */
#define PIN_LINK_RX   16   /* UART2 RX  <- gateway TX (GPIO17) */
#define PIN_LINK_TX   17   /* UART2 TX  -> gateway RX (GPIO18) */
#define PIN_DHT       4
#define PIN_LDR       34
#define PIN_I2C_SDA   21   /* ENS160 */
#define PIN_I2C_SCL   22
#define PIN_LED       25
#define PIN_US_TRIG   26   /* HC-SR04 TRIG (3.3 V is enough) */
#define PIN_US_ECHO   27   /* HC-SR04 ECHO is 5 V: use a divider (1k top / 2k bottom) */

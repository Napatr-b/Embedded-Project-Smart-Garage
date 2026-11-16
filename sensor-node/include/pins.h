#pragma once
/* Sensor node pins: PROVISIONAL, see docs/pin-map.md.
 * GPIO34-39 are input-only (good for LDR on ADC1). Avoid ADC2 pins: they can't be used while Wi-Fi is on (OTA/E6). */
#define PIN_LINK_RX   16   /* UART2 RX  <- gateway TX */
#define PIN_LINK_TX   17   /* UART2 TX  -> gateway RX */
#define PIN_DHT       4
#define PIN_LDR       34
#define PIN_I2C_SDA   21   /* ENS160 */
#define PIN_I2C_SCL   22
#define PIN_LED       25

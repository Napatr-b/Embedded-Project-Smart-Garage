# Pin map (PROVISIONAL)

Map everything on paper before wiring. Update `gateway/include/pins.h` and `sensor-node/include/pins.h` to match.

## Gateway: ESP32-S3-DevKitC-1-N8R8

Reserved: GPIO35-37 (PSRAM), 19/20 (USB), 43/44 (UART0 console), 0/3/45/46 (strapping).

| Function | GPIO | Notes |
|---|---|---|
| UART1 TX -> node RX | 17 | |
| UART1 RX <- node TX | 18 | |
| Ultrasonic entry TRIG / ECHO | 4 / 5 | ECHO is 5 V on HC-SR04: use a divider |
| Ultrasonic bay TRIG / ECHO | 6 / 7 | same |
| IR obstacle OUT | 15 | ISR input (E2) |
| Servo PWM | 16 | servo powered from its own 5 V supply, common GND |
| Camera OV2640 DVP | TBD | ~16 signals: XCLK, PCLK, VSYNC, HREF, D0-D7, SDA, SCL, (PWDN, RESET) |

## Sensor Node: ESP32 DOIT DevKit V1

| Function | GPIO | Notes |
|---|---|---|
| UART2 RX <- gateway TX | 16 | |
| UART2 TX -> gateway RX | 17 | |
| DHT22 data | 4 | 10k pull-up |
| LDR (divider) | 34 | ADC1, input-only |
| ENS160 SDA / SCL | 21 / 22 | I2C |
| LED | 25 | via resistor/transistor |

Both boards must share GND.

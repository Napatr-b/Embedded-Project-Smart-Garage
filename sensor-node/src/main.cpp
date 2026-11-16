/*
 * Smart Garage: Sensor Node firmware (ESP32 DOIT, Arduino)
 * Reads DHT22, LDR, ENS160, drives LED lighting, sends readings to the gateway
 * over UART with ACK/retry (E4).
 *
 * Status: TEMPLATE. ENS160 is stubbed. Not yet built or tested.
 */
#include <Arduino.h>
#include <DHT.h>
#include "garage_protocol.h"
#include "pins.h"

#define NODE_ID          1
#define LINK_BAUD        115200
#define SEND_PERIOD_MS   5000
#define ACK_TIMEOUT_MS   200
#define MAX_RETRIES      3
#define LDR_DARK_THRESHOLD 1500   /* TODO: calibrate on the real model */

HardwareSerial Link(2);
DHT dht(PIN_DHT, DHT22);
static uint16_t seq = 0;

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

static gp_readings_t read_sensors() {
  gp_readings_t r = {};
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) r.temp_c_x100   = (int16_t)(t * 100);
  if (!isnan(h)) r.humidity_x100 = (uint16_t)(h * 100);

  r.ldr_raw = analogRead(PIN_LDR);

  /* TODO: ENS160 over I2C (eco2_ppm, tvoc_ppb, aqi) */

  r.led_on = r.ldr_raw < LDR_DARK_THRESHOLD;   /* TODO: check LDR wiring direction (dark = low or high?) */
  digitalWrite(PIN_LED, r.led_on ? HIGH : LOW);
  return r;
}

void setup() {
  Serial.begin(115200);
  Link.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  pinMode(PIN_LED, OUTPUT);
  dht.begin();
  Serial.println("sensor node started");
}

void loop() {
  gp_readings_t r = read_sensors();
  uint8_t frame[GP_MAX_FRAME];
  size_t len = gp_build_frame(frame, GP_MSG_READINGS, NODE_ID, seq, &r, sizeof(r));

  bool acked = false;
  for (int attempt = 0; attempt <= MAX_RETRIES && !acked; attempt++) {
    Link.write(frame, len);
    acked = wait_for_ack(seq);
    if (!acked) Serial.printf("seq %u: no ACK (attempt %d)\n", seq, attempt + 1);
  }
  Serial.printf("seq %u %s\n", seq, acked ? "ACK" : "FAILED");
  seq++;

  delay(SEND_PERIOD_MS);
}

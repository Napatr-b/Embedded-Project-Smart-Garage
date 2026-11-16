/*
 * Smart Garage: Sensor Node firmware (ESP32 DOIT, Arduino)
 *
 * Each feature lives in its own file:
 *   link.cpp         UART link to the gateway: framing, ACK/retry (E4)
 *   ultrasonic.cpp   HC-SR04 distance
 *   env_sensors.cpp  DHT22, LDR, ENS160 (stub), LED lighting
 * This file only schedules them. Turn features on/off in include/config.h.
 *
 * Status: ultrasonic -> gateway path written; env sensors not wired/tested yet.
 */
#include <Arduino.h>
#include "config.h"
#include "garage_protocol.h"
#include "link.h"
#include "ultrasonic.h"
#include "env_sensors.h"

static void print_result(bool acked, int tries) {
  LinkStats s = link_stats();
  Serial.printf("%s (tries=%d) delivered %lu/%lu\n", acked ? "ACK" : "NO ACK", tries,
                (unsigned long)s.acked, (unsigned long)s.sent);
}

#if FEATURE_ULTRASONIC
static uint32_t last_us_ms = 0;

static void send_ultrasonic() {
  gp_ultrasonic_t m = ultrasonic_read();
  uint16_t seq = link_next_seq();
  int tries = 0;
  bool acked = link_send(GP_MSG_ULTRASONIC, &m, sizeof(m), &tries);

  if (m.status == GP_US_OK) Serial.printf("[node] seq=%u dist=%u mm ", seq, m.distance_mm);
  else                      Serial.printf("[node] seq=%u dist=NO_ECHO ", seq);
  print_result(acked, tries);
}
#endif

#if FEATURE_ENV
static uint32_t last_env_ms = 0;

static void send_env() {
  gp_readings_t r = env_read();
  uint16_t seq = link_next_seq();
  int tries = 0;
  bool acked = link_send(GP_MSG_READINGS, &r, sizeof(r), &tries);

  Serial.printf("[node] seq=%u T=%.2f H=%.2f ldr=%u led=%u ", seq, r.temp_c_x100 / 100.0,
                r.humidity_x100 / 100.0, r.ldr_raw, r.led_on);
  print_result(acked, tries);
}
#endif

void setup() {
  Serial.begin(115200);
  link_begin();
#if FEATURE_ULTRASONIC
  ultrasonic_begin();
#endif
#if FEATURE_ENV
  env_begin();
#endif
  Serial.printf("\n[node] sensor node %d started (ultrasonic=%d env=%d)\n", NODE_ID,
                FEATURE_ULTRASONIC, FEATURE_ENV);
}

void loop() {
  uint32_t now = millis();
#if FEATURE_ULTRASONIC
  if (now - last_us_ms >= ULTRASONIC_PERIOD_MS) {
    last_us_ms = now;
    send_ultrasonic();
  }
#endif
#if FEATURE_ENV
  if (now - last_env_ms >= ENV_PERIOD_MS) {
    last_env_ms = now;
    send_env();
  }
#endif
  delay(5);
}

#include "ultrasonic.h"
#include <Arduino.h>
#include "pins.h"

#define ECHO_TIMEOUT_US 30000   /* ~5 m round trip; HC-SR04 is good to ~4 m */

void ultrasonic_begin() {
  pinMode(PIN_US_TRIG, OUTPUT);
  pinMode(PIN_US_ECHO, INPUT);
  digitalWrite(PIN_US_TRIG, LOW);
}

gp_ultrasonic_t ultrasonic_read() {
  gp_ultrasonic_t m = {0, GP_US_NO_ECHO};

  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);   /* >=10 us pulse starts a measurement */
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);

  unsigned long echo_us = pulseIn(PIN_US_ECHO, HIGH, ECHO_TIMEOUT_US);   /* 0 on timeout */
  if (echo_us) {
    /* sound ~343 m/s = 0.343 mm/us; divide by 2 for the round trip */
    m.distance_mm = (uint16_t)(echo_us * 343UL / 2000UL);
    m.status = GP_US_OK;
  }
  return m;
}

const char *ultrasonic_diagnose() {
  delay(60);   /* HC-SR04 needs ~60 ms between measurements */
  if (digitalRead(PIN_US_ECHO) == HIGH)
    return "ECHO is HIGH while idle: 2k resistor to GND missing, or ECHO wire on the wrong pin";

  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);

  /* Watch the ECHO pin directly: did it rise at all, and for how long? */
  uint32_t start = micros();
  while (digitalRead(PIN_US_ECHO) == LOW) {
    if (micros() - start > 50000)
      return "ECHO never went HIGH: check VCC on VIN (5 V), TRIG -> D26, ECHO -> 1k -> D27, GND";
  }
  uint32_t rise = micros();
  while (digitalRead(PIN_US_ECHO) == HIGH) {
    /* Many HC-SR04 clones hold ECHO HIGH for ~150-200 ms when no echo returns, so one of these is
     * normal (moving or angled target). Only every reading doing it points at a fault. */
    if (micros() - rise > 60000)
      return "ECHO held HIGH > 60 ms: no echo came back (normal now and then; if EVERY reading says "
             "this, ECHO is shorted to VCC or the sensor is faulty)";
  }
  uint32_t width = micros() - rise;
  if (width > 30000)
    return "echo longer than 30 ms: nothing in range, point it at a wall < 2 m away";
  return "this reading worked: the earlier miss was a one-off (soft or angled target?)";
}

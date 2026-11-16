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

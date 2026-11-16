#include "env_sensors.h"
#include <Arduino.h>
#include <DHT.h>
#include "config.h"
#include "pins.h"

static DHT dht(PIN_DHT, DHT22);

void env_begin() {
  pinMode(PIN_LED, OUTPUT);
  dht.begin();
}

gp_readings_t env_read() {
  gp_readings_t r = {};
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) r.temp_c_x100   = (int16_t)(t * 100);
  if (!isnan(h)) r.humidity_x100 = (uint16_t)(h * 100);

  r.ldr_raw = analogRead(PIN_LDR);

  /* TODO: ENS160 over I2C (eco2_ppm, tvoc_ppb, aqi). Left 0 = "no reading". */

  r.led_on = r.ldr_raw < LDR_DARK_THRESHOLD;   /* TODO: check LDR wiring direction (dark = low or high?) */
  digitalWrite(PIN_LED, r.led_on ? HIGH : LOW);
  return r;
}

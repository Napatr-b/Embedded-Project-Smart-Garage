#pragma once
/* HC-SR04 ultrasonic distance sensor. */
#include "garage_protocol.h"

void ultrasonic_begin();

/* Triggers one measurement. Blocks up to ~30 ms waiting for the echo. */
gp_ultrasonic_t ultrasonic_read();

/* Call after a NO_ECHO reading. Does one slow, instrumented measurement and returns a short
 * wiring hint (what the ECHO pin actually did). Blocks ~120 ms. */
const char *ultrasonic_diagnose();

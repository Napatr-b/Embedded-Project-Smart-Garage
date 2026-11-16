#pragma once
/* Environment sensors (DHT22, LDR, ENS160) and the LED lighting they control. */
#include "garage_protocol.h"

void env_begin();

/* Reads all environment sensors and switches the LED on when it's dark. */
gp_readings_t env_read();

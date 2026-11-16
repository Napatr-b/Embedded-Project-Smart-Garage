#pragma once
#if __has_include("config.local.h")
#include "config.local.h"
#else
#include "config.example.h"
#endif
#ifndef GARAGE_NETWORK_ENABLED
#define GARAGE_NETWORK_ENABLED 0
#endif
#ifndef GARAGE_SIMULATED_HARDWARE
#define GARAGE_SIMULATED_HARDWARE 0
#endif
#ifndef GARAGE_LOCAL_TEST
#define GARAGE_LOCAL_TEST 0
#endif
#ifndef GARAGE_EXPECTED_FLASH_BYTES
#define GARAGE_EXPECTED_FLASH_BYTES 8388608
#endif
#if GARAGE_LOCAL_TEST && !GARAGE_SIMULATED_HARDWARE
#error Local test transport requires simulated hardware with actuators disabled
#endif
#include "contract.generated.h"

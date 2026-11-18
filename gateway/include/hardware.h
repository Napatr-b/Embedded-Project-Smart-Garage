#pragma once
#include <cmath>
#include <cstdint>
namespace garage {
// One reading of every sensor. NAN / -1 = not available (no driver yet), sent as JSON null.
struct SensorSnapshot {
  double temperature = NAN, humidity = NAN, ldrRaw = NAN, aqi = NAN, tvocPpb = NAN, eco2Ppm = NAN,
         distanceCm = NAN;
  int occupied = -1, ledOn = -1;
  int64_t ultrasonicSampleMs = -1;
};
// Simulated values when simulated=true; otherwise all fields unavailable until real drivers exist.
SensorSnapshot sampleHardware(bool simulated, int64_t monotonicMs);
} // namespace garage

#include "hardware.h"
#include "node_link.h"
namespace garage {
SensorSnapshot sampleHardware(bool simulated, int64_t ms) {
  SensorSnapshot s;
  if (simulated) {
    const double t = ms / 1000.0;
    s.temperature = 29.4 + std::sin(t / 25) * 0.7;
    s.humidity = 63 + std::sin(t / 31) * 2;
    s.ldrRaw = 1820 + std::sin(t / 27) * 280;
    s.ledOn = s.ldrRaw < 1700;
    s.aqi = 2;
    s.tvocPpb = 125;
    s.eco2Ppm = 540;
    s.distanceCm = 180;
    s.occupied = 0;
  } else {
    // Real data: whatever the sensor node sent recently over UART. Gateway-side sensors
    // (bay ultrasonic, IR) will be added here when their drivers exist.
    node_link::fill(s, ms);
  }
  return s;
}
} // namespace garage

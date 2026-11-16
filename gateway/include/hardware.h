#pragma once
#include <cmath>
#include <cstdint>
namespace garage {
struct SensorSnapshot {
  double temperature = NAN, humidity = NAN, ldrRaw = NAN, aqi = NAN, tvocPpb = NAN, eco2Ppm = NAN,
         distanceCm = NAN;
  int occupied = -1, ledOn = -1;
};
class SensorUart {
public:
  virtual ~SensorUart() = default;
  virtual bool poll(SensorSnapshot &) = 0;
};
class IrInput {
public:
  virtual ~IrInput() = default;
  virtual bool available() const = 0;
  virtual bool triggered() const = 0;
};
class Ultrasonic {
public:
  virtual ~Ultrasonic() = default;
  virtual double distanceCm() const = 0;
};
class ServoOutput {
public:
  virtual ~ServoOutput() = default;
  virtual bool commandAngle(float) = 0;
  virtual void stop() = 0;
};
enum class FaceVerdict { Unavailable, Denied, Granted };
class FaceRecognition {
public:
  virtual ~FaceRecognition() = default;
  virtual FaceVerdict poll() = 0;
};
// Disabled adapters do not call GPIO, camera or PWM APIs.
class DisabledSensorUart final : public SensorUart {
public:
  bool poll(SensorSnapshot &) override { return false; }
};
class DisabledIr final : public IrInput {
public:
  bool available() const override { return false; }
  bool triggered() const override { return false; }
};
class DisabledUltrasonic final : public Ultrasonic {
public:
  double distanceCm() const override { return NAN; }
};
class DisabledServo final : public ServoOutput {
public:
  bool commandAngle(float) override { return false; }
  void stop() override {}
};
class DisabledFaceRecognition final : public FaceRecognition {
public:
  FaceVerdict poll() override { return FaceVerdict::Unavailable; }
};
SensorSnapshot sampleHardware(bool simulated, int64_t monotonicMs);
} // namespace garage

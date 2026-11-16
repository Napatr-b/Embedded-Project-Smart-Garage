#pragma once
#include <cstddef>
namespace garage {
// Gateway/backend owns HTTPS credentials. No real request is made in this round.
struct HistorySink {
  virtual ~HistorySink() = default;
  virtual bool appendTelemetry(const char *json) = 0;
  virtual bool appendEvent(const char *json) = 0;
};
struct NotificationSink {
  virtual ~NotificationSink() = default;
  virtual bool notify(const char *eventJson) = 0;
};
class UnconfiguredHistorySink final : public HistorySink {
public:
  bool appendTelemetry(const char *) override { return false; }
  bool appendEvent(const char *) override { return false; }
};
class UnconfiguredLineSink final : public NotificationSink {
public:
  bool notify(const char *) override { return false; }
};
} // namespace garage

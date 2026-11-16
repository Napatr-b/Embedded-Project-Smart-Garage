#include "door_controller.h"
#include <algorithm>
namespace garage {
void DoorController::advance(int64_t now) {
  const int direction = state_ == Door::Opening ? 1 : state_ == Door::Closing ? -1 : 0;
  position_ = std::clamp(position_ + direction * (now - updated_) / 28.0, 0.0, 100.0);
  updated_ = now;
}
const char *DoorController::request(const Command &c, int64_t now) {
  if (std::strcmp(c.action, "open") && std::strcmp(c.action, "close") && std::strcmp(c.action, "stop"))
    return "invalid_action";
  if (!simulation_) return "actuator_disabled_no_wiring_or_feedback";
  advance(now);
  if (!std::strcmp(c.action, "stop")) {
    if (state_ != Door::Fault) state_ = position_ == 0 ? Door::Closed : position_ == 100 ? Door::Open : Door::Stopped;
    clearPending();
    return nullptr;
  }
  if (state_ == Door::Fault || state_ == Door::Unknown) return "fault_or_unknown_requires_local_inspection";
  targetOpen_ = !std::strcmp(c.action, "open");
  std::strncpy(pending_, c.id, 128);
  started_ = now;
  updated_ = now;
  state_ = targetOpen_ ? Door::Opening : Door::Closing;
  return nullptr;
}
const char *DoorController::tick(int64_t now, bool connected) {
  if (!pending_[0]) return nullptr;
  advance(now);
  if (!connected) { state_ = Door::Fault; return "connection_lost"; }
  if (now - started_ >= kMovementTimeoutMs) { state_ = Door::Fault; return "movement_timeout"; }
  if (simulation_ && position_ == (targetOpen_ ? 100 : 0)) {
    state_ = targetOpen_ ? Door::Open : Door::Closed;
    return "completed";
  }
  return nullptr;
}
} // namespace garage

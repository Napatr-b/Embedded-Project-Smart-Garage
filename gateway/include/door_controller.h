#pragma once
#include <cstdint>
#include "contract.generated.h"
#include <cstring>
namespace garage {
enum class Door { Unknown, Closed, Opening, Open, Closing, Stopped, Fault };
inline const char *doorName(Door d) {
  switch (d) {
  case Door::Closed:
    return "Closed";
  case Door::Opening:
    return "Opening";
  case Door::Open:
    return "Open";
  case Door::Closing:
    return "Closing";
  case Door::Stopped:
    return "Stopped";
  case Door::Fault:
    return "Fault";
  default:
    return nullptr;
  }
}
struct Command {
  char id[129]{};
  char action[8]{};
  int64_t sentMs = 0;
};
// Bounded replay cache. Time watermark also rejects commands older than an accepted command.
class CommandGuard {
  char seen[32][129]{};
  unsigned index = 0;
  int64_t watermark = 0;

public:
  const char *check(const Command &c, bool retained, bool synced, int64_t now) {
    if (retained)
      return "retained_command";
    if (!synced)
      return "clock_unsynced";
    if (c.sentMs > now + kFutureSkewMs || c.sentMs < now - kCommandTtlMs)
      return "expired_or_future_command";
    for (const auto &id : seen)
      if (id[0] && std::strcmp(id, c.id) == 0)
        return "duplicate_command";
    if (c.sentMs <= watermark)
      return "replayed_or_out_of_order";
    return nullptr;
  }
  void remember(const Command &c) {
    std::strncpy(seen[index], c.id, 128);
    index = (index + 1) % 32;
    watermark = c.sentMs;
  }
};
class DoorController {
  Door state_;
  bool simulation_;
  int64_t started_ = 0;
  int64_t updated_ = 0;
  double position_ = 0;
  void advance(int64_t now);
  char pending_[129]{};
  bool targetOpen_ = false;

public:
  explicit DoorController(bool simulation)
      : state_(simulation ? Door::Closed : Door::Unknown), simulation_(simulation) {}
  double position() const { return position_; }
  Door state() const { return state_; }
  const char *pendingId() const { return pending_; }
  bool simulated() const { return simulation_; }
  const char *request(const Command &c, int64_t monotonicMs);
  // Returns completed / movement_timeout. Never infers real position from a servo angle.
  const char *tick(int64_t monotonicMs, bool connected);
  void clearPending() { pending_[0] = 0; }
  void feedbackForTest(Door d) {
    if (simulation_)
      state_ = d;
  }
};
} // namespace garage

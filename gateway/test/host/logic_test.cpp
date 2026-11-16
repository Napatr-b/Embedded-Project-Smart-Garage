#include "door_controller.h"
#include <cassert>
#include <iostream>
using namespace garage;
static Command command(const char *id, const char *action, int64_t time) {
  Command c{};
  std::strncpy(c.id, id, 128);
  std::strncpy(c.action, action, 7);
  c.sentMs = time;
  return c;
}
int main() {
  const int64_t now = 100000;
  CommandGuard guard;
  auto open = command("one", "open", now);
  assert(!guard.check(open, false, true, now));
  guard.remember(open);
  assert(std::strcmp(guard.check(open, false, true, now), "duplicate_command") == 0);
  assert(std::strcmp(guard.check(command("two", "close", now - 9000), false, true, now),
                     "expired_or_future_command") == 0);
  assert(std::strcmp(guard.check(command("two", "close", now + 3000), false, true, now),
                     "expired_or_future_command") == 0);
  assert(std::strcmp(guard.check(command("two", "close", now + 1), true, true, now),
                     "retained_command") == 0);
  assert(std::strcmp(guard.check(command("two", "close", now + 1), false, false, now),
                     "clock_unsynced") == 0);
  assert(std::strcmp(guard.check(command("two", "close", now - 1), false, true, now),
                     "replayed_or_out_of_order") == 0);
  // Even after bounded cache eviction the timestamp watermark rejects replay.
  for (int i = 0; i < 40; i++) {
    char id[20];
    std::snprintf(id, sizeof(id), "id%d", i);
    auto c = command(id, "open", now + i + 1);
    guard.remember(c);
  }
  assert(guard.check(open, false, true, now + 40));
  DoorController physical(false);
  assert(physical.state() == Door::Unknown);
  assert(physical.request(open, 0));
  assert(physical.state() == Door::Unknown);
  DoorController demo(true);
  assert(!demo.request(open, 0));
  assert(demo.state() == Door::Opening);
  assert(!demo.request(command("repeat", "open", now + 1), 1));
  assert(demo.tick(2799, true) == nullptr);
  assert(std::strcmp(demo.tick(2800, true), "completed") == 0);
  assert(demo.state() == Door::Open);
  demo.clearPending();
  assert(!demo.request(command("close", "close", now + 2), 3000));
  assert(!demo.request(command("stop", "stop", now + 3), 3100));
  assert(demo.state() == Door::Stopped);
  assert(demo.tick(7000, true) == nullptr);
  assert(!demo.request(open, 7001));
  // All four stop/resume combinations, proportional remaining travel.
  for (bool opening : {true, false}) for (bool resumeOpen : {true, false}) {
    DoorController motion(true);
    int64_t time = 0;
    if (!opening) {
      assert(!motion.request(command("seed", "open", now), time));
      assert(motion.tick(time += 2800, true)); motion.clearPending();
    }
    assert(!motion.request(command("half", opening ? "open" : "close", now), time));
    assert(!motion.tick(time += 1400, true));
    assert(motion.position() == 50);
    assert(!motion.request(command("hold", "stop", now), time));
    assert(motion.state() == Door::Stopped);
    assert(!motion.tick(time += 10000, true)); assert(motion.position() == 50);
    assert(!motion.request(command("resume", resumeOpen ? "open" : "close", now), time));
    assert(!motion.tick(time += 700, true)); assert(motion.position() == (resumeOpen ? 75 : 25));
    assert(motion.tick(time += 700, true));
    assert(motion.state() == (resumeOpen ? Door::Open : Door::Closed));
    motion.clearPending(); assert(!motion.tick(time += 10000, true));
  }
  DoorController reversal(true);
  assert(!reversal.request(open, 0)); assert(!reversal.tick(700, true));
  for (int i = 0; i < 100; ++i) {
    assert(!reversal.request(command("repeat", i % 2 ? "close" : "open", now), 700));
    assert(reversal.position() == 25);
  }
  assert(reversal.tick(1400, true)); assert(reversal.state() == Door::Closed);
  DoorController timeout(true);
  assert(!timeout.request(open, 0));
  assert(std::strcmp(timeout.tick(6000, true), "movement_timeout") == 0);
  assert(timeout.state() == Door::Fault);
  DoorController offline(true);
  assert(!offline.request(open, 0));
  assert(std::strcmp(offline.tick(100, false), "connection_lost") == 0);
  assert(offline.state() == Door::Fault);
  std::cout << "C++ firmware logic checks passed (host only; no hardware)\n";
}

#include "protocol.h"
#include <cassert>
#include <cstring>
#include <ctime>
#include <iostream>
using namespace garage;
int main() {
  tzset();
  assert(!clockSynced());
  markTimeSynchronized();
  assert(clockSynced());
  auto *env = envelope();
  assert(cJSON_IsTrue(cJSON_GetObjectItem(env, "timeSynced")));
  assert(cJSON_IsString(cJSON_GetObjectItem(env, "timestamp")));
  assert(cJSON_GetObjectItem(env, "schemaVersion")->valueint == 2);
  cJSON_Delete(env);
  const char *valid =
      "{\"schemaVersion\":2,\"deviceId\":\"garage-01\",\"timestamp\":\"2026-11-16T03:00:00.000Z\","
      "\"timeSynced\":true,\"commandId\":\"test-1\",\"action\":\"open\"}";
  Command c{};
  assert(parseCommand(valid, strlen(valid), c) == nullptr);
  assert(strcmp(c.action, "open") == 0);
  assert(c.sentMs == 1794798000000LL);
  for (const char *bad : {"{bad", "null", "[]", "{}", "{\"timestamp\":null}"}) {
    Command badCommand{};
    assert(parseCommand(bad, strlen(bad), badCommand));
  }
  Command nested{};
  const char *deep = "[[[[[0]]]]]";
  assert(strcmp(parseCommand(deep, strlen(deep), nested), "excessive_json_nesting") == 0);
  std::string s = valid;
  auto replace = [&](const char *from, const char *to) {
    s = valid;
    s.replace(s.find(from), strlen(from), to);
    Command out{};
    return parseCommand(s.c_str(), s.size(), out);
  };
  assert(replace("garage-01", "other-id"));
  assert(replace("schemaVersion\":2", "schemaVersion\":1"));
  assert(replace("timeSynced\":true", "timeSynced\":false"));
  assert(replace("open", "fly"));
  assert(replace("2026-11-16", "2026-02-30"));
  assert(replace("03:00:00", "99:00:00"));
  assert(replace(".000Z", ".-01Z"));
  assert(replace(".000Z", ".000+07:00"));
  assert(parseCommand(valid, 1025, c));
  std::cout << "C++ firmware JSON parser checks passed (host only)\n";
}

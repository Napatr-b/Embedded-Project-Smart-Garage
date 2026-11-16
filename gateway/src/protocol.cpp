#include "protocol.h"
#include "config.h"
#include <cstdio>
#include <ctime>
#include <sys/time.h>
#include <atomic>
namespace garage {
int64_t utcMs() {
  timeval tv{};
  gettimeofday(&tv, nullptr);
  return int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}
static std::atomic<bool> sntpConfirmed{false};
void markTimeSynchronized() { sntpConfirmed = true; }
bool clockSynced() { return sntpConfirmed && utcMs() > 1767225600000LL; }
void formatTimestamp(char *out, size_t size) {
  if (!clockSynced()) {
    out[0] = 0;
    return;
  }
  const auto ms = utcMs();
  const time_t sec = ms / 1000;
  tm t{};
#ifdef _WIN32
  gmtime_s(&t, &sec);
#else
  gmtime_r(&sec, &t);
#endif
  char date[24];
  strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &t);
  snprintf(out, size, "%s.%03dZ", date, int(ms % 1000));
}
cJSON *envelope() {
  auto *p = cJSON_CreateObject();
  cJSON_AddNumberToObject(p, "schemaVersion", kSchemaVersion);
  cJSON_AddStringToObject(p, "deviceId", GARAGE_DEVICE_ID);
  char ts[32];
  formatTimestamp(ts, sizeof(ts));
  if (ts[0])
    cJSON_AddStringToObject(p, "timestamp", ts);
  else
    cJSON_AddNullToObject(p, "timestamp");
  cJSON_AddBoolToObject(p, "timeSynced", clockSynced());
  cJSON_AddBoolToObject(p, "simulatedHardware", GARAGE_SIMULATED_HARDWARE);
  return p;
}
const char *parseCommand(const char *text, size_t length, Command &out) {
  if (!length || length > 1024)
    return "invalid_length";
  // A small command is flat JSON; bound nesting before the recursive cJSON parser.
  bool quoted = false, escaped = false;
  unsigned depth = 0;
  for (size_t i = 0; i < length; i++) {
    const char ch = text[i];
    if (quoted) {
      if (escaped)
        escaped = false;
      else if (ch == '\\')
        escaped = true;
      else if (ch == '"')
        quoted = false;
      continue;
    }
    if (ch == '"') {
      quoted = true;
      continue;
    }
    if (ch == '{' || ch == '[') {
      if (++depth > 4)
        return "excessive_json_nesting";
    } else if (ch == '}' || ch == ']') {
      if (!depth)
        return "invalid_json";
      --depth;
    }
  }
  const char *end = nullptr;
  auto *root = cJSON_ParseWithLengthOpts(text, length + 1, &end, true);
  if (!root)
    return "invalid_json";
  const char *reason = nullptr;
  auto *synced = cJSON_GetObjectItemCaseSensitive(root, "timeSynced");
  auto *ver = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
  auto *device = cJSON_GetObjectItemCaseSensitive(root, "deviceId");
  auto *id = cJSON_GetObjectItemCaseSensitive(root, "commandId");
  auto *action = cJSON_GetObjectItemCaseSensitive(root, "action");
  auto *ts = cJSON_GetObjectItemCaseSensitive(root, "timestamp");
  if (cJSON_IsString(id) && strlen(id->valuestring) > 0 && strlen(id->valuestring) <= 128)
    std::strncpy(out.id, id->valuestring, 128);
  if (!cJSON_IsTrue(synced) || !cJSON_IsObject(root) || !cJSON_IsNumber(ver) ||
      ver->valuedouble != kSchemaVersion || !cJSON_IsString(device) ||
      strcmp(device->valuestring, GARAGE_DEVICE_ID) != 0)
    reason = "wrong_schema_or_device";
  else if (!cJSON_IsString(id) || !id->valuestring[0] || strlen(id->valuestring) > 128)
    reason = "invalid_command_id";
  else if (!cJSON_IsString(action) ||
           (strcmp(action->valuestring, "open") && strcmp(action->valuestring, "close") &&
            strcmp(action->valuestring, "stop")))
    reason = "invalid_action";
  else if (!cJSON_IsString(ts) || strlen(ts->valuestring) != 24)
    reason = "invalid_timestamp";
  else {
    std::strncpy(out.id, id->valuestring, 128);
    std::strncpy(out.action, action->valuestring, 7);
    for (size_t i = 0; i < 24; i++) {
      const char expected = i == 4 || i == 7     ? '-'
                            : i == 10            ? 'T'
                            : i == 13 || i == 16 ? ':'
                            : i == 19            ? '.'
                            : i == 23            ? 'Z'
                                                 : 0;
      if ((expected && ts->valuestring[i] != expected) ||
          (!expected && (ts->valuestring[i] < '0' || ts->valuestring[i] > '9'))) {
        cJSON_Delete(root);
        return "invalid_timestamp";
      }
    }
    int y, mo, d, h, mi, s, ms, n = 0;
    if (sscanf(ts->valuestring, "%4d-%2d-%2dT%2d:%2d:%2d.%3dZ%n", &y, &mo, &d, &h, &mi, &s, &ms,
               &n) != 7 ||
        n != 24 || y < 2026 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59)
      reason = "invalid_timestamp";
    else {
      tm t{};
      t.tm_year = y - 1900;
      t.tm_mon = mo - 1;
      t.tm_mday = d;
      t.tm_hour = h;
      t.tm_min = mi;
      t.tm_sec = s;
      const time_t seconds = mktime(&t);
      char check[24];
      strftime(check, sizeof(check), "%Y-%m-%dT%H:%M:%S", &t);
      if (strncmp(check, ts->valuestring, 19))
        reason = "invalid_calendar_date";
      else
        out.sentMs = int64_t(seconds) * 1000 + ms;
    }
  }
  cJSON_Delete(root);
  return reason;
}
} // namespace garage

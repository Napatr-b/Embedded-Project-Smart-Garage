#pragma once
#include "door_controller.h"
#include "cJSON.h"
namespace garage {
int64_t utcMs();
bool clockSynced();
void markTimeSynchronized();
void formatTimestamp(char *out, size_t size);
cJSON *envelope();
// Caller owns returned JSON. Uses shared draft v1 field names with the web.
const char *parseCommand(const char *text, size_t length, Command &out);
} // namespace garage

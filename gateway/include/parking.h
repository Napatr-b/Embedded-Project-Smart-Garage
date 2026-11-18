#pragma once
#include <cstdint>

namespace garage::parking {
void begin();
void poll(int64_t monotonicMs);
}

#pragma once

// Busy wait helpers based on the generic counter. Use only during early
// bring-up or inside code that cannot sleep.

#include <cstdint>

namespace og::time {

void DelayUs(uint64_t microseconds) noexcept;
void DelayMs(uint64_t milliseconds) noexcept;

}  // namespace og::time

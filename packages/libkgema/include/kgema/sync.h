#pragma once

// Futex primitives shared across processes through mapped memory.

#include "kgema/types.h"

namespace kgema {

constexpr i64 kFutexWait = 0;
constexpr i64 kFutexWake = 1;

// Blocks while the 32-bit word at address equals expected. Returns 0 when
// the value changed, or a negative errno.
[[nodiscard]] i64 futex_wait(i64 address, i64 expected) noexcept;

// Wakes up to count waiters on the same physical word. Returns the number
// woken or a negative errno.
[[nodiscard]] i64 futex_wake(i64 address, i64 count) noexcept;

}  // namespace kgema

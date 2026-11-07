#pragma once

// Futex primitives. The address must point into a mapped user region and is
// shared across processes when the underlying physical page is shared.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::sync {

// Blocks while the 32-bit word at address equals expected. Returns ok when
// the value changed or immediately when it already differs.
[[nodiscard]] Status wait(int* address, int expected) noexcept;

// Wakes up to count waiters on the same physical word. Returns the number of
// waiters actually woken.
[[nodiscard]] Result<u64> wake(int* address, u64 count) noexcept;

}  // namespace gemapi::sync

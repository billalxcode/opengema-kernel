#pragma once

// Monotonic time since boot and sleeping, without libc.

#include "kgema/types.h"

namespace kgema {

[[nodiscard]] u64 now_ms() noexcept;

// Returns true when the sleep completed.
[[nodiscard]] bool sleep_ms(u64 milliseconds) noexcept;

}  // namespace kgema

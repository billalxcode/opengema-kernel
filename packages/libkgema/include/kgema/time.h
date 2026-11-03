#pragma once

// Monotonic time since boot and sleeping, without libc.

#include "kgema/types.h"

namespace kgema {

[[nodiscard]] u64 now_ms() noexcept;

// Returns true when the sleep completed.
[[nodiscard]] bool sleep_ms(u64 milliseconds) noexcept;

// Wall clock in Unix seconds. Returns false when no RTC is registered.
[[nodiscard]] bool wall_seconds(u64& seconds) noexcept;
[[nodiscard]] bool set_wall_seconds(u64 seconds) noexcept;

}  // namespace kgema

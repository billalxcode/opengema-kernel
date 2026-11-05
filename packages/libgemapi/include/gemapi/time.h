#pragma once

// Monotonic time since boot and sleeping for applications.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::time {

// Milliseconds since the kernel started.
[[nodiscard]] u64 now_ms() noexcept;

// Blocks the calling program for the requested duration.
[[nodiscard]] Status sleep_ms(u64 milliseconds) noexcept;

// Wall clock in Unix seconds. Fails with not_supported without an RTC.
[[nodiscard]] Result<u64> wall_seconds() noexcept;
[[nodiscard]] Status set_wall_seconds(u64 seconds) noexcept;

// Wall clock plus the runtime timezone offset.
[[nodiscard]] Result<u64> local_seconds() noexcept;
[[nodiscard]] Status set_timezone_minutes(i64 minutes) noexcept;
[[nodiscard]] Result<i64> timezone_minutes() noexcept;

}  // namespace gemapi::time

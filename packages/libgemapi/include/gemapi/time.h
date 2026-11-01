#pragma once

// Monotonic time since boot and sleeping for applications.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::time {

// Milliseconds since the kernel started.
[[nodiscard]] u64 now_ms() noexcept;

// Blocks the calling program for the requested duration.
[[nodiscard]] Status sleep_ms(u64 milliseconds) noexcept;

}  // namespace gemapi::time

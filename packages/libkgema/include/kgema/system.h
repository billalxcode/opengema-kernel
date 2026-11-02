#pragma once

// System power requests and kernel log level control. Power calls return
// only when the kernel rejects them.

#include "kgema/types.h"

namespace kgema {

[[nodiscard]] i64 shutdown() noexcept;
[[nodiscard]] i64 reboot() noexcept;
[[nodiscard]] i64 get_log_level() noexcept;
[[nodiscard]] i64 set_log_level(i64 level) noexcept;

}  // namespace kgema

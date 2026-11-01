#pragma once

// System power requests. These return only when the kernel rejects them.

#include "kgema/types.h"

namespace kgema {

[[nodiscard]] i64 shutdown() noexcept;
[[nodiscard]] i64 reboot() noexcept;

}  // namespace kgema

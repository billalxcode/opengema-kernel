#pragma once

// Process lifecycle helpers.

#include "kgema/types.h"

namespace kgema {

[[noreturn]] void exit(int status) noexcept;
[[nodiscard]] u64 getpid() noexcept;
[[nodiscard]] i64 spawn(const char* path, const char* const* argv = nullptr,
                        usize argc = 0) noexcept;
[[nodiscard]] i64 wait(i64 pid, int* status) noexcept;

}  // namespace kgema

#pragma once

// Process lifecycle helpers.

#include "kgema/types.h"

namespace kgema {

[[noreturn]] void exit(int status) noexcept;
[[nodiscard]] u64 getpid() noexcept;
[[nodiscard]] i64 spawn(const char* path, const char* const* argv = nullptr,
                        usize argc = 0) noexcept;
[[nodiscard]] i64 wait(i64 pid, int* status) noexcept;
[[nodiscard]] i64 fork() noexcept;
[[nodiscard]] i64 exec(const char* path, const char* const* argv = nullptr,
                       usize argc = 0) noexcept;
[[nodiscard]] bool uid(u64& value) noexcept;
[[nodiscard]] bool setuid(u64 value) noexcept;
[[nodiscard]] bool gid(u64& value) noexcept;
[[nodiscard]] bool setgid(u64 value) noexcept;

// Scheduling priority, 0 highest and 31 lowest.
[[nodiscard]] bool priority(u64& value) noexcept;
[[nodiscard]] bool setpriority(u64 value) noexcept;

}  // namespace kgema

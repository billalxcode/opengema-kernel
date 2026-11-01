#pragma once

// Canonical owner of the syscall ABI.
// svc #0, number in x8, arguments in x0..x5, result in x0.

#include <cstdint>

namespace og::syscall {

constexpr uint64_t kSysWrite = 1;
constexpr uint64_t kSysRead = 2;
constexpr uint64_t kSysExit = 3;
constexpr uint64_t kSysYield = 4;

void SyscallInit() noexcept;

[[nodiscard]] bool UserExited() noexcept;

}  // namespace og::syscall

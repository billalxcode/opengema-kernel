#pragma once

// Process lifecycle. Each application runs in its own process and address
// space, so a crash stops only that application.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::process {

[[noreturn]] void exit(int status) noexcept;
[[nodiscard]] u64 id() noexcept;

// Starts a program from the filesystem. argv[0] is the program name.
[[nodiscard]] Result<u64> spawn(const char* path, const char* const* argv = nullptr,
                                usize argc = 0) noexcept;

// Waits for a child and stores its exit status.
[[nodiscard]] Status wait(u64 pid, int& status) noexcept;

}  // namespace gemapi::process

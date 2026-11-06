#pragma once

// User memory helpers.

#include "kgema/types.h"

namespace kgema {

// Maps one anonymous, writable region. Returns nullptr on failure.
[[nodiscard]] void* mmap(usize size) noexcept;

// Maps one anonymous, writable, executable region. Used only by the
// dynamic linker to place shared objects.
[[nodiscard]] void* mmap_executable(usize size) noexcept;

// Reserves a lazily populated anonymous writable region.
[[nodiscard]] void* mmap_lazy(usize size) noexcept;

// Returns true when the kernel accepted the unmap.
[[nodiscard]] bool munmap(void* address, usize size) noexcept;

// Rewrites permissions for a mapped region. Requesting executable and
// writable together is rejected by the kernel (W^X).
[[nodiscard]] bool mprotect(void* address, usize size, bool executable,
                            bool writable) noexcept;

}  // namespace kgema

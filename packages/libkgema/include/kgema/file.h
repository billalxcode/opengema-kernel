#pragma once

// File helpers. Paths are NUL terminated strings in the caller address space.

#include "kgema/types.h"

namespace kgema {

constexpr u64 kOpenRead = 0;

struct Stat {
  u64 type;
  u64 size;
};

// Returns a descriptor, or -1 on error.
[[nodiscard]] i64 open(const char* path, u64 flags = kOpenRead) noexcept;
[[nodiscard]] bool close(i64 descriptor) noexcept;
[[nodiscard]] i64 read(i64 descriptor, void* buffer, usize size) noexcept;
[[nodiscard]] bool seek(i64 descriptor, u64 offset) noexcept;
[[nodiscard]] bool stat(const char* path, Stat& result) noexcept;

}  // namespace kgema

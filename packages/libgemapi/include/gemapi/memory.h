#pragma once

// Anonymous user memory. The API never exposes physical addresses.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::memory {

// Maps one anonymous, writable region. The result is page aligned.
[[nodiscard]] Result<void*> map(usize size) noexcept;

// Maps one anonymous, writable region without allocating pages until first
// access.
[[nodiscard]] Result<void*> map_lazy(usize size) noexcept;

// Maps one anonymous, writable region shared with children across fork.
[[nodiscard]] Result<void*> map_shared(usize size) noexcept;

// Maps a private snapshot of a regular file. The descriptor only needs to be
// valid during the call; offset must be page aligned.
[[nodiscard]] Result<void*> map_file(i64 descriptor, u64 offset,
                                     usize size) noexcept;

// Releases a region previously returned by map.
[[nodiscard]] Status unmap(void* address, usize size) noexcept;

}  // namespace gemapi::memory

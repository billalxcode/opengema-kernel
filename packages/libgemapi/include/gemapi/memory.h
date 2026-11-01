#pragma once

// Anonymous user memory. The API never exposes physical addresses.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::memory {

// Maps one anonymous, writable region. The result is page aligned.
[[nodiscard]] Result<void*> map(usize size) noexcept;

// Releases a region previously returned by map.
[[nodiscard]] Status unmap(void* address, usize size) noexcept;

}  // namespace gemapi::memory

#pragma once

// Canonical owner of kernel heap allocation.
// Size classes up to 2 KiB use slabs; larger requests take whole pages.
// Every block carries a header so free knows the class without a lookup.

#include <cstdint>

#include "og/core/status.h"

namespace og::mem {

struct HeapStats {
  uint64_t pages;
  uint64_t allocations;
  uint64_t frees;
};

[[nodiscard]] Status HeapInit() noexcept;

[[nodiscard]] Result<void*> HeapAlloc(uint64_t size) noexcept;
[[nodiscard]] Result<void*> HeapZalloc(uint64_t size) noexcept;
[[nodiscard]] Result<void*> HeapRealloc(void* pointer, uint64_t size) noexcept;
[[nodiscard]] Status HeapFree(void* pointer) noexcept;

[[nodiscard]] HeapStats GetHeapStats() noexcept;

}  // namespace og::mem

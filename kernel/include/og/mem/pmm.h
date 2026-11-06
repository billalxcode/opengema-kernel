#pragma once

// Canonical owner of physical page allocation.
// Bitmap over the usable ranges of BootInfo. Only ConventionalMemory is
// released, so firmware owned memory and the live kernel stack stay intact.

#include <cstdint>

#include "og/core/bootinfo.h"
#include "og/core/status.h"

namespace og::mem {

constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kMaxFrames = 1ULL << 20U;  // 4 GiB of 4 KiB frames
// Below this many free frames the syscall layer may kill the process that
// failed to allocate, freeing its address space on reap.
constexpr uint64_t kOomReserveFrames = 1024;  // 4 MiB

[[nodiscard]] Status PmmInit(const BootInfo& info) noexcept;

[[nodiscard]] Result<uint64_t> PmmAllocPage() noexcept;
[[nodiscard]] Result<uint64_t> PmmAllocPages(uint64_t count) noexcept;
[[nodiscard]] Status PmmFreePage(uint64_t address) noexcept;
[[nodiscard]] Status PmmFreePages(uint64_t address, uint64_t count) noexcept;
[[nodiscard]] Status PmmRetainPage(uint64_t address) noexcept;

[[nodiscard]] uint64_t PmmFreeFrames() noexcept;
[[nodiscard]] uint64_t PmmTotalFrames() noexcept;
[[nodiscard]] bool PmmReady() noexcept;

}  // namespace og::mem

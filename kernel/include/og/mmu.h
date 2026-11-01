#pragma once

// Canonical owner of MMU activation. The virtual memory manager owns the
// tables; this layer only programs MAIR, TCR, TTBR0, and SCTLR.

#include <cstdint>

#include "og/status.h"

namespace og::arch::aarch64 {

// Installs ttbr0 as the EL1 translation table base and enables the MMU,
// data cache, and instruction cache. ttbr0 must be page aligned.
[[nodiscard]] Status ActivateAddressSpace(uint64_t ttbr0) noexcept;

[[nodiscard]] bool IsMmuEnabled() noexcept;

}  // namespace og::arch::aarch64

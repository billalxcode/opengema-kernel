#pragma once

// Canonical owner of MMU activation. The virtual memory manager owns the
// tables; this layer only programs MAIR, TCR, TTBR0, and SCTLR.

#include <cstdint>

#include "og/core/status.h"

namespace og::arch::aarch64 {

// Installs ttbr0 as the EL1 translation table base and enables the MMU,
// data cache, and instruction cache. ttbr0 must be page aligned.
[[nodiscard]] Status ActivateAddressSpace(uint64_t ttbr0) noexcept;

// Switches the active user table with an ASID tag. Kernel entries are shared
// by every table, so this is safe to call with interrupts disabled.
void SwitchAddressSpace(uint64_t ttbr0, uint16_t asid) noexcept;

[[nodiscard]] bool IsMmuEnabled() noexcept;

// True when ID_AA64MMFR1_EL1 reports PAN support.
[[nodiscard]] bool PanSupported() noexcept;

// Clears SCTLR_EL1.SPAN so exception entry from EL0 sets PSTATE.PAN.
void EnablePan() noexcept;

}  // namespace og::arch::aarch64

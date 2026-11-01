#pragma once

// Canonical owner of TLB maintenance.
// The per-VA variant lands with the virtual memory manager in phase 3.

#include "og/arch/barriers.h"

namespace og::arch::aarch64 {

inline void InvalidateTlbAllLocal() noexcept {
  __asm__ volatile("tlbi vmalle1" ::: "memory");
  DsbSy();
  InstructionSync();
}

}  // namespace og::arch::aarch64

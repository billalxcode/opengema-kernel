#pragma once

// Canonical owner of TLB maintenance.

#include "og/arch/barriers.h"

namespace og::arch::aarch64 {

inline void InvalidateTlbAllLocal() noexcept {
  __asm__ volatile("tlbi vmalle1" ::: "memory");
  DsbSy();
  InstructionSync();
}

inline void InvalidateTlbVaLocal(uint64_t virtual_address) noexcept {
  const uint64_t operand = virtual_address >> 12U;
  __asm__ volatile("tlbi vae1, %0" ::"r"(operand) : "memory");
}

inline void InvalidateTlbSync() noexcept {
  DsbSy();
  InstructionSync();
}

}  // namespace og::arch::aarch64

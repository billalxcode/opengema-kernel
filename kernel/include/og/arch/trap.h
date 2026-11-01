#pragma once

// Canonical owner of the exception vector table and the trap entry ABI.
// Phase 4 extends dispatch with IRQ routing.

#include <cstdint>

namespace og::arch::aarch64 {

// Register state pushed by the assembly entry stub. The layout is part of
// the asm contract in vectors.S: x[0..30], elr, spsr.
struct TrapFrame {
  uint64_t x[31];
  uint64_t elr;
  uint64_t spsr;
};

void InstallVectorTable() noexcept;

}  // namespace og::arch::aarch64

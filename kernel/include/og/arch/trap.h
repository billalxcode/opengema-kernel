#pragma once

// Trap frame pushed by the assembly entry stub in vectors.S.
// Layout is part of the asm contract: x[0..30], elr, spsr.

#include <cstdint>

namespace og::arch::aarch64 {

struct TrapFrame {
  uint64_t x[31];
  uint64_t elr;
  uint64_t spsr;
};

}  // namespace og::arch::aarch64

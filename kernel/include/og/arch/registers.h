#pragma once

// Canonical owner of AArch64 system register access.
// Fase 2 extends this file. Nothing else may read these registers
// with inline asm.

#include <cstdint>

namespace og::arch::aarch64 {

[[nodiscard]] inline uint64_t ReadEsrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, esr_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadFarEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, far_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadElrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, elr_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadSpsrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, spsr_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadVbarEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, vbar_el1" : "=r"(value));
  return value;
}

inline void DisableInterrupts() noexcept {
  __asm__ volatile("msr daifset, #0xf" ::: "memory");
}

inline void EnableInterrupts() noexcept {
  __asm__ volatile("msr daifclr, #0xf" ::: "memory");
}

[[noreturn]] inline void HaltForever() noexcept {
  for (;;) {
    __asm__ volatile("wfi");
  }
}

}  // namespace og::arch::aarch64

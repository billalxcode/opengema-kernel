#pragma once

// Canonical owner of AArch64 system register access.
// Nothing else may read or write these registers with inline asm.

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

inline void WriteVbarEl1(uint64_t value) noexcept {
  __asm__ volatile("msr vbar_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadSctlrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, sctlr_el1" : "=r"(value));
  return value;
}

inline void WriteSctlrEl1(uint64_t value) noexcept {
  __asm__ volatile("msr sctlr_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadTcrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, tcr_el1" : "=r"(value));
  return value;
}

inline void WriteTcrEl1(uint64_t value) noexcept {
  __asm__ volatile("msr tcr_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadTtbr0El1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(value));
  return value;
}

inline void WriteTtbr0El1(uint64_t value) noexcept {
  __asm__ volatile("msr ttbr0_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadMairEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, mair_el1" : "=r"(value));
  return value;
}

inline void WriteMairEl1(uint64_t value) noexcept {
  __asm__ volatile("msr mair_el1, %0" ::"r"(value) : "memory");
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

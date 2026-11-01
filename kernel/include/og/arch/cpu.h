#pragma once

// Canonical owner of CPU identification registers.

#include <cstdint>

namespace og::arch::aarch64 {

[[nodiscard]] inline uint64_t CurrentEl() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, currentel" : "=r"(value));
  return (value >> 2U) & 0x3U;
}

// Aff0..Aff2 of MPIDR_EL1. Unique per core on QEMU virt.
[[nodiscard]] inline uint64_t CpuId() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
  return value & 0x00FFFFFFU;
}

}  // namespace og::arch::aarch64

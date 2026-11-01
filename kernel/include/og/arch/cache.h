#pragma once

// Canonical owner of cache maintenance.
// Cache line size comes from CTR_EL0, never hardcoded.

#include <cstdint>

#include "og/arch/barriers.h"

namespace og::arch::aarch64 {

[[nodiscard]] inline uint64_t DataCacheLineBytes() noexcept {
  uint64_t ctr = 0;
  __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
  const uint64_t words = (ctr >> 16U) & 0xFU;
  return 4U << words;
}

// Cleans and invalidates every line covering [begin, end).
inline void CleanInvalidateDcacheRange(uint64_t begin, uint64_t end) noexcept {
  if (end <= begin) {
    return;
  }
  const uint64_t line = DataCacheLineBytes();
  uint64_t address = begin & ~(line - 1U);
  while (address < end) {
    __asm__ volatile("dc civac, %0" ::"r"(address) : "memory");
    address += line;
  }
  DsbSy();
}

inline void InvalidateIcacheAll() noexcept {
  __asm__ volatile("ic iallu" ::: "memory");
  DsbSy();
  InstructionSync();
}

}  // namespace og::arch::aarch64

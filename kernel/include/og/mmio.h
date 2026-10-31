#pragma once

#include <concepts>
#include <cstdint>

namespace og {

template <typename T>
concept MmioWord = std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>;

inline void MmioWrite32(uint64_t addr, uint32_t value) noexcept {
  auto* p = reinterpret_cast<volatile uint32_t*>(addr);
  if (p == nullptr) {
    return;
  }
  *p = value;
  __asm__ volatile("dsb sy" ::: "memory");
}

inline uint32_t MmioRead32(uint64_t addr) noexcept {
  auto* p = reinterpret_cast<volatile const uint32_t*>(addr);
  if (p == nullptr) {
    return 0;
  }
  uint32_t v = *p;
  __asm__ volatile("dsb sy" ::: "memory");
  return v;
}

}  // namespace og

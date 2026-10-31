#include "og/mmio.h"

namespace og::io {

namespace {

inline void DeviceBarrier() noexcept {
  __asm__ volatile("dsb sy" ::: "memory");
}

}  // namespace

uint32_t MmioRead32(uint64_t address) noexcept {
  if (address == 0) {
    return 0;
  }
  const auto* reg = reinterpret_cast<volatile const uint32_t*>(address);
  const uint32_t value = *reg;
  DeviceBarrier();
  return value;
}

void MmioWrite32(uint64_t address, uint32_t value) noexcept {
  if (address == 0) {
    return;
  }
  auto* reg = reinterpret_cast<volatile uint32_t*>(address);
  *reg = value;
  DeviceBarrier();
}

uint64_t MmioRead64(uint64_t address) noexcept {
  if (address == 0) {
    return 0;
  }
  const auto* reg = reinterpret_cast<volatile const uint64_t*>(address);
  const uint64_t value = *reg;
  DeviceBarrier();
  return value;
}

void MmioWrite64(uint64_t address, uint64_t value) noexcept {
  if (address == 0) {
    return;
  }
  auto* reg = reinterpret_cast<volatile uint64_t*>(address);
  *reg = value;
  DeviceBarrier();
}

uint8_t MmioRead8(uint64_t address) noexcept {
  if (address == 0) {
    return 0;
  }
  const auto* reg = reinterpret_cast<volatile const uint8_t*>(address);
  const uint8_t value = *reg;
  DeviceBarrier();
  return value;
}

void MmioWrite8(uint64_t address, uint8_t value) noexcept {
  if (address == 0) {
    return;
  }
  auto* reg = reinterpret_cast<volatile uint8_t*>(address);
  *reg = value;
  DeviceBarrier();
}

}  // namespace og::io

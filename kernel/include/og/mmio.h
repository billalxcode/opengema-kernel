#pragma once

// Canonical owner of memory mapped IO access.
// No other module may touch volatile pointers or emit barriers around MMIO.

#include <cstdint>

namespace og::io {

[[nodiscard]] uint32_t MmioRead32(uint64_t address) noexcept;
void MmioWrite32(uint64_t address, uint32_t value) noexcept;

[[nodiscard]] uint64_t MmioRead64(uint64_t address) noexcept;
void MmioWrite64(uint64_t address, uint64_t value) noexcept;

[[nodiscard]] uint8_t MmioRead8(uint64_t address) noexcept;
void MmioWrite8(uint64_t address, uint8_t value) noexcept;

}  // namespace og::io

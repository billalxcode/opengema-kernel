#pragma once

// Canonical little-endian readers for on-disk and firmware formats.
// Every parser uses this header instead of a local byte reader.

#include <cstdint>

namespace og::endian {

[[nodiscard]] constexpr uint16_t ReadLe16(const uint8_t* data) noexcept {
  return static_cast<uint16_t>(
    static_cast<uint16_t>(data[0]) |
    static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8U));
}

[[nodiscard]] constexpr uint32_t ReadLe32(const uint8_t* data) noexcept {
  uint32_t value = 0;
  for (uint32_t index = 0; index < 4U; ++index) {
    value |= static_cast<uint32_t>(data[index]) << (index * 8U);
  }
  return value;
}

[[nodiscard]] constexpr uint64_t ReadLe64(const uint8_t* data) noexcept {
  uint64_t value = 0;
  for (uint32_t index = 0; index < 8U; ++index) {
    value |= static_cast<uint64_t>(data[index]) << (index * 8U);
  }
  return value;
}

}  // namespace og::endian

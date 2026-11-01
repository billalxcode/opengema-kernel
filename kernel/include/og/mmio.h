#pragma once

// Canonical owner of memory mapped IO access.
// Every access is bounds checked. No other module may touch volatile
// pointers or emit barriers around MMIO.

#include <cstdint>

#include "og/status.h"

namespace og::io {

class MmioRegion {
 public:
  constexpr MmioRegion() noexcept = default;
  constexpr MmioRegion(uint64_t base, uint64_t size) noexcept
    : base_(base), size_(size) {}

  [[nodiscard]] constexpr bool valid() const noexcept { return base_ != 0 && size_ != 0; }
  [[nodiscard]] constexpr uint64_t base() const noexcept { return base_; }
  [[nodiscard]] constexpr uint64_t size() const noexcept { return size_; }

  [[nodiscard]] constexpr bool Contains(uint64_t offset, uint64_t width) const noexcept {
    if (width == 0) {
      return false;
    }
    if (offset > size_) {
      return false;
    }
    return width <= (size_ - offset);
  }

  [[nodiscard]] Result<uint8_t> Read8(uint64_t offset) const noexcept;
  [[nodiscard]] Result<uint16_t> Read16(uint64_t offset) const noexcept;
  [[nodiscard]] Result<uint32_t> Read32(uint64_t offset) const noexcept;
  [[nodiscard]] Result<uint64_t> Read64(uint64_t offset) const noexcept;

  [[nodiscard]] Status Write8(uint64_t offset, uint8_t value) const noexcept;
  [[nodiscard]] Status Write16(uint64_t offset, uint16_t value) const noexcept;
  [[nodiscard]] Status Write32(uint64_t offset, uint32_t value) const noexcept;
  [[nodiscard]] Status Write64(uint64_t offset, uint64_t value) const noexcept;

  // Waits for device writes to complete. Use after configuring a device,
  // not on every access.
  void Sync() const noexcept;

 private:
  uint64_t base_ = 0;
  uint64_t size_ = 0;
};

}  // namespace og::io

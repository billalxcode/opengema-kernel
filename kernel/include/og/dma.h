#pragma once

// Canonical owner of DMA buffer management.
// Identity mapping today: device addresses equal CPU addresses. When a
// higher half layout lands, translation goes here and nowhere else.

#include <cstdint>

#include "og/status.h"

namespace og::dma {

using DmaAddress = uint64_t;

enum class DmaDirection : uint32_t {
  kToDevice = 0,
  kFromDevice = 1,
  kBoth = 2,
};

struct DmaBuffer {
  void* cpu_address;
  DmaAddress device_address;
  uint64_t size;
};

// Allocates page backed, zeroed memory suitable for device access.
// alignment must be a power of two up to 4096.
[[nodiscard]] Result<DmaBuffer> AllocCoherent(uint64_t size, uint64_t alignment) noexcept;
[[nodiscard]] Status FreeCoherent(DmaBuffer buffer) noexcept;

// Prepares a CPU buffer for device access and returns its device address.
[[nodiscard]] Result<DmaAddress> MapStreaming(void* cpu_address, uint64_t size,
                                             DmaDirection direction) noexcept;
[[nodiscard]] Status UnmapStreaming(void* cpu_address, uint64_t size,
                                    DmaDirection direction) noexcept;

// Manual cache maintenance for drivers that own the timing.
[[nodiscard]] Status SyncForDevice(void* cpu_address, uint64_t size) noexcept;
[[nodiscard]] Status SyncForCpu(void* cpu_address, uint64_t size) noexcept;

}  // namespace og::dma

#pragma once

// Canonical owner of DMA buffer management.
// Identity mapping today: device addresses equal CPU addresses. When a
// higher half layout lands, translation goes here and nowhere else.

#include <cstdint>

#include "og/core/status.h"

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

struct Stats {
  uint64_t allocations;
  uint64_t frees;
  uint64_t live_bytes;
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
[[nodiscard]] Stats GetStats() noexcept;

// Fixed size coherent block pool. Useful for drivers that need a stable
// number of requests or descriptors.
class Pool {
 public:
  Pool() noexcept = default;
  ~Pool() noexcept = default;

  Pool(const Pool&) = delete;
  Pool& operator=(const Pool&) = delete;

  [[nodiscard]] Status init(uint64_t block_size, uint32_t block_count) noexcept;
  [[nodiscard]] Result<DmaBuffer> alloc() noexcept;
  [[nodiscard]] Status free(DmaBuffer buffer) noexcept;
  [[nodiscard]] Status release() noexcept;

  [[nodiscard]] uint32_t available() const noexcept { return free_count_; }
  [[nodiscard]] bool is_ready() const noexcept { return ready_; }

 private:
  DmaBuffer backing_{};
  uint32_t block_size_ = 0;
  uint32_t block_count_ = 0;
  uint32_t free_count_ = 0;
  void* free_list_ = nullptr;
  uint64_t used_bitmap_ = 0;
  bool ready_ = false;
};

}  // namespace og::dma

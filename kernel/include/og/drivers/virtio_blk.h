#pragma once

// virtio-blk driver on top of the modern PCI transport. Polled requests,
// no interrupt path yet.

#include <cstdint>
#include <span>

#include "og/arch/spinlock.h"
#include "og/core/status.h"
#include "og/dma/dma.h"
#include "og/drivers/pci.h"
#include "og/drivers/virtio.h"

namespace og::drivers::virtio {

struct BlkStats {
  uint64_t reads;
  uint64_t writes;
  uint64_t flushes;
};

class Blk {
 public:
  Blk() noexcept = default;
  ~Blk() = default;

  Blk(const Blk&) = delete;
  Blk& operator=(const Blk&) = delete;

  [[nodiscard]] Status init(const pci::Info& info) noexcept;

  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] uint64_t capacity_sectors() const noexcept { return capacity_; }
  [[nodiscard]] uint16_t queue_size() const noexcept { return queue_.size(); }
  [[nodiscard]] uint64_t mmio_base() const noexcept {
    return transport_.common_base();
  }
  [[nodiscard]] uint64_t dma_bytes() const noexcept {
    return request_memory_.size + queue_.memory_bytes();
  }

  // Reads whole 512 byte sectors. buffer.size() must be a multiple of 512
  // and at most 4096 bytes.
  [[nodiscard]] Status read(uint64_t sector, std::span<uint8_t> buffer) noexcept;

  // Writes whole 512 byte sectors with the same size rules as read.
  [[nodiscard]] Status write(uint64_t sector,
                             std::span<const uint8_t> buffer) noexcept;

  // Flushes the device write cache.
  [[nodiscard]] Status flush() noexcept;

 private:
  [[nodiscard]] Status validate_transfer(uint64_t sector, uint64_t size,
                                         uint64_t& sectors) noexcept;

  PciTransport transport_;
  Queue queue_;
  dma::DmaBuffer request_memory_{};
  arch::aarch64::IrqSpinlock lock_;
  uint64_t capacity_ = 0;
  bool ready_ = false;
};

[[nodiscard]] bool IsVirtioBlock(const pci::Info& info) noexcept;
[[nodiscard]] BlkStats GetBlkStats() noexcept;

}  // namespace og::drivers::virtio

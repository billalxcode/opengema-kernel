#pragma once

// virtio-blk driver on top of the modern PCI transport. Polled requests,
// no interrupt path yet.

#include <cstdint>
#include <span>

#include "og/dma.h"
#include "og/pci.h"
#include "og/status.h"
#include "og/virtio.h"

namespace og::drivers::virtio {

class Blk {
 public:
  Blk() noexcept = default;
  ~Blk() = default;

  Blk(const Blk&) = delete;
  Blk& operator=(const Blk&) = delete;

  [[nodiscard]] Status init(const pci::Info& info) noexcept;

  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] uint64_t capacity_sectors() const noexcept { return capacity_; }
  [[nodiscard]] uint16_t queue_size() const noexcept { return queue_size_; }

  // Reads whole 512 byte sectors. buffer.size() must be a multiple of 512
  // and at most 4096 bytes.
  [[nodiscard]] Status read(uint64_t sector, std::span<uint8_t> buffer) noexcept;

 private:
  PciTransport transport_;
  dma::DmaBuffer queue_memory_{};
  dma::DmaBuffer request_memory_{};
  uint64_t avail_offset_ = 0;
  uint64_t used_offset_ = 0;
  uint64_t used_bytes_ = 0;
  uint16_t queue_size_ = 0;
  uint16_t avail_index_ = 0;
  uint16_t used_index_ = 0;
  uint64_t capacity_ = 0;
  bool ready_ = false;
};

[[nodiscard]] bool IsVirtioBlock(const pci::Info& info) noexcept;

}  // namespace og::drivers::virtio

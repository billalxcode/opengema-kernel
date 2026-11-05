#pragma once

// Modern virtio PCI transport (virtio 1.0 and later). Covers the common
// configuration structure, feature negotiation, queue setup, and notify.

#include <cstdint>

#include "og/core/status.h"
#include "og/dma/dma.h"
#include "og/drivers/pci.h"
#include "og/io/mmio.h"

namespace og::drivers::virtio {

constexpr uint64_t kFeatureVersion1 = 1ULL << 32U;

class PciTransport {
 public:
  PciTransport() noexcept = default;
  ~PciTransport() = default;

  PciTransport(const PciTransport&) = delete;
  PciTransport& operator=(const PciTransport&) = delete;

  [[nodiscard]] Status init(const pci::Info& info) noexcept;

  [[nodiscard]] Status reset() noexcept;
  [[nodiscard]] Status begin_init() noexcept;
  [[nodiscard]] Status negotiate(uint64_t desired_features) noexcept;

  [[nodiscard]] Result<uint16_t> select_queue(uint16_t index) noexcept;
  [[nodiscard]] Status configure_queue(uint16_t index, uint64_t descriptor_address,
                                       uint64_t available_address,
                                       uint64_t used_address) noexcept;
  [[nodiscard]] Status set_driver_ok() noexcept;

  [[nodiscard]] Status notify(uint16_t index) noexcept;

  [[nodiscard]] io::MmioRegion device_config() const noexcept { return device_; }
  [[nodiscard]] uint64_t common_base() const noexcept { return common_.base(); }
  [[nodiscard]] bool is_ready() const noexcept { return ready_; }

 private:
  [[nodiscard]] Status write_status(uint8_t value) noexcept;
  [[nodiscard]] Result<uint8_t> read_status() noexcept;

  io::MmioRegion common_;
  io::MmioRegion notify_;
  io::MmioRegion device_;
  uint32_t notify_multiplier_ = 0;
  uint16_t notify_offset_ = 0;
  uint8_t status_ = 0;
  bool ready_ = false;
};

// One buffer in a descriptor chain. The CPU and device address are kept
// separate because coherent allocations happen to be identity mapped today,
// but a driver must not assume that.
struct QueueBuffer {
  void* cpu_address;
  uint64_t device_address;
  uint32_t length;
  bool device_writes;
};

// Split virtqueue with one request in flight. Enough for the polled block
// and GPU control paths; it intentionally has no interrupt support yet.
class Queue {
 public:
  static constexpr uint32_t kMaxChain = 4;
  static constexpr uint32_t kMaxBuffers = 64;

  Queue() noexcept = default;
  ~Queue() = default;

  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;

  [[nodiscard]] Status init(PciTransport& transport, uint16_t index,
                            uint16_t max_size) noexcept;

  // Publishes a descriptor chain and notifies the device.
  [[nodiscard]] Status submit(const QueueBuffer* buffers, uint32_t count) noexcept;

  // Publishes each buffer as its own available chain. Used by devices such
  // as virtio-input that consume one descriptor per event.
  [[nodiscard]] Status submit_each(const QueueBuffer* buffers,
                                   uint32_t count) noexcept;

  // Waits for the device to consume the chain, then invalidates the
  // buffers so the CPU sees device writes. Times out after one second.
  [[nodiscard]] Status wait() noexcept;

  // Checks whether the device consumed the chain without blocking.
  [[nodiscard]] Status poll(bool& ready) noexcept;

  [[nodiscard]] uint32_t used_length() const noexcept { return used_length_; }
  [[nodiscard]] uint16_t size() const noexcept { return size_; }
  [[nodiscard]] uint64_t memory_bytes() const noexcept { return memory_.size; }
  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] bool has_pending() const noexcept { return pending_count_ != 0U; }

 private:
  PciTransport* transport_ = nullptr;
  dma::DmaBuffer memory_{};
  uint64_t avail_offset_ = 0;
  uint64_t used_offset_ = 0;
  uint64_t used_bytes_ = 0;
  uint16_t index_ = 0;
  uint16_t size_ = 0;
  uint16_t avail_index_ = 0;
  uint16_t used_index_ = 0;
  QueueBuffer pending_[kMaxBuffers]{};
  uint32_t pending_count_ = 0;
  uint32_t used_length_ = 0;
  bool ready_ = false;
};

}  // namespace og::drivers::virtio

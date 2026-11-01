#pragma once

// Modern virtio PCI transport (virtio 1.0 and later). Covers the common
// configuration structure, feature negotiation, queue setup, and notify.

#include <cstdint>

#include "og/mmio.h"
#include "og/pci.h"
#include "og/status.h"

namespace og::drivers::virtio {

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

}  // namespace og::drivers::virtio

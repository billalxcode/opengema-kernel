#pragma once

// Virtio input transport. Produces generic HID events for keyboard and
// pointer devices; the report decoder stays transport independent.

#include <cstdint>

#include "og/core/status.h"
#include "og/drivers/hid.h"
#include "og/drivers/pci.h"
#include "og/drivers/virtio.h"

namespace og::drivers::virtio {

[[nodiscard]] bool IsVirtioInput(const pci::Info& info) noexcept;

class Input {
 public:
  Input() noexcept = default;
  ~Input() = default;

  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;

  [[nodiscard]] Status init(const pci::Info& info, hid::EventFn sink,
                            void* context) noexcept;
  [[nodiscard]] Status poll() noexcept;

  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] uint64_t mmio_base() const noexcept {
    return transport_.common_base();
  }

 private:
  [[nodiscard]] Status submit_events() noexcept;

  PciTransport transport_;
  Queue queue_;
  dma::DmaBuffer events_{};
  hid::EventFn sink_ = nullptr;
  void* context_ = nullptr;
  bool submitted_ = false;
  bool ready_ = false;
};

}  // namespace og::drivers::virtio

#pragma once

// GICv3 driver for QEMU virt. Single core scope: distributor,
// redistributor of the calling core, and the system register CPU interface.

#include <cstdint>

#include "og/irq.h"
#include "og/mmio.h"
#include "og/status.h"

namespace og::drivers::irq {

class Gicv3 {
 public:
  Gicv3() noexcept = default;
  ~Gicv3() noexcept = default;

  Gicv3(const Gicv3&) = delete;
  Gicv3& operator=(const Gicv3&) = delete;

  [[nodiscard]] Status init(uint64_t distributor_base, uint64_t redistributor_base) noexcept;
  [[nodiscard]] Status enable_irq(uint32_t intid) noexcept;
  [[nodiscard]] Status disable_irq(uint32_t intid) noexcept;

  [[nodiscard]] uint32_t acknowledge() noexcept;
  void end(uint32_t intid) noexcept;

  [[nodiscard]] bool is_ready() const noexcept { return ready_; }

 private:
  io::MmioRegion distributor_;
  io::MmioRegion redistributor_;
  bool ready_ = false;
};

[[nodiscard]] events::InterruptController MakeController(Gicv3& gic) noexcept;

}  // namespace og::drivers::irq

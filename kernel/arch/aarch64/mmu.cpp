#include "og/mmu.h"

namespace og::arch::aarch64 {

Status Mmu::init_identity() noexcept {
  if (enabled_) {
    return Status::kOk;
  }
  // TODO(opengema): implement TTBR0 identity map for RAM + UART.
  // Next step: build L0/L1 tables, set MAIR/TCR/TTBR, enable SCTLR.M.
  return Status::kNotImpl;
}

}  // namespace og::arch::aarch64

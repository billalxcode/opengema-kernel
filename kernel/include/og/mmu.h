#pragma once

#include "og/status.h"

namespace og::arch::aarch64 {

class Mmu {
 public:
  Mmu() noexcept = default;

  [[nodiscard]] Status init_identity() noexcept;
  [[nodiscard]] bool is_enabled() const noexcept { return enabled_; }

 private:
  bool enabled_ = false;
};

}  // namespace og::arch::aarch64

#pragma once

// Kernel page tables. Phase 2 scope: identity map of RAM plus device space
// below 1 GiB. Phase 3 replaces the policy with a real virtual memory manager.

#include <cstdint>

#include "og/status.h"

namespace og::arch::aarch64 {

class Mmu {
 public:
  Mmu() noexcept = default;
  ~Mmu() noexcept = default;

  Mmu(const Mmu&) = delete;
  Mmu& operator=(const Mmu&) = delete;

  // Installs identity tables covering [ram_base, ram_base + ram_size) as
  // Normal write back memory, and everything below ram_base as Device.
  // ram_base must be 1 GiB aligned.
  [[nodiscard]] Status init_identity(uint64_t ram_base, uint64_t ram_size) noexcept;

  [[nodiscard]] bool is_enabled() const noexcept { return enabled_; }

 private:
  bool enabled_ = false;
};

}  // namespace og::arch::aarch64

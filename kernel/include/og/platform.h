#pragma once

// Canonical owner of the board descriptor. Values are defaults for the
// QEMU virt machine; discovery from firmware tables comes later.

#include <cstdint>

namespace og::platform {

struct Range {
  uint64_t base;
  uint64_t size;
};

struct Platform {
  const char* name;
  Range uart0;
  Range gic_distributor;
  Range gic_redistributor;
  Range pci_ecam;
  uint64_t ram_base;
  uint32_t cpu_count;
  uint32_t psci_conduit;  // 0 = hvc, 1 = smc
};

constexpr uint32_t kPsciConduitHvc = 0;
constexpr uint32_t kPsciConduitSmc = 1;

[[nodiscard]] const Platform& Current() noexcept;

}  // namespace og::platform

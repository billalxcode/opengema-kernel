#pragma once

// Canonical owner of the board descriptor. Values are defaults for the
// QEMU virt machine; discovery from firmware tables comes later.

#include <cstdint>

#include "og/core/bootinfo.h"
#include "og/core/status.h"

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
  Range rtc;
  Range pci_ecam;
  uint64_t ram_base;
  uint64_t uart_clock;
  uint32_t cpu_count;
  uint32_t psci_conduit;  // 0 = hvc, 1 = smc
};

constexpr uint32_t kPsciConduitHvc = 0;
constexpr uint32_t kPsciConduitSmc = 1;

// Copies the board defaults, then overrides what the device tree reports.
[[nodiscard]] Status PlatformInit(const BootInfo& info) noexcept;
[[nodiscard]] const Platform& Current() noexcept;

}  // namespace og::platform

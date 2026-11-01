#pragma once

// Canonical owner of secondary core bring up and per-CPU data.
// Scheduling stays single core: secondaries only handle IPIs and park.

#include <cstdint>

#include "og/status.h"

namespace og::smp {

constexpr uint32_t kMaxCpus = 4;
constexpr uint32_t kKernelStackPages = 4;
constexpr uint32_t kIpiIrq = 3;

struct CpuInfo {
  uint64_t mpidr;
  uint64_t stack_top;
  uint32_t index;
  uint32_t online;
};

[[nodiscard]] Status SmpInit() noexcept;
[[nodiscard]] Status StartSecondaries() noexcept;

[[nodiscard]] uint32_t OnlineCount() noexcept;
[[nodiscard]] uint32_t CpuCount() noexcept;
[[nodiscard]] uint32_t CurrentIndex() noexcept;
[[nodiscard]] CpuInfo* CurrentCpu() noexcept;
[[nodiscard]] CpuInfo* FindCpuFor(uint64_t mpidr) noexcept;

}  // namespace og::smp

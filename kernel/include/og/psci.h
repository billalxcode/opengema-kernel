#pragma once

// Canonical owner of the PSCI calls used for secondary core bring up.

#include <cstdint>

#include "og/status.h"

namespace og::psci {

// Probes HVC first, then SMC, and remembers which conduit responds.
[[nodiscard]] Status PsciInit() noexcept;

[[nodiscard]] Result<uint64_t> CpuOn(uint64_t mpidr, uint64_t entry_address,
                                     uint64_t context) noexcept;

[[nodiscard]] uint32_t Version() noexcept;

}  // namespace og::psci

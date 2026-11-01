#pragma once

// Canonical owner of the PSCI calls used for secondary core bring up.

#include <cstdint>

#include "og/status.h"

namespace og::psci {

// Tries the preferred conduit first, then the other one, and remembers
// which responds.
[[nodiscard]] Status PsciInit(uint32_t preferred_conduit) noexcept;

[[nodiscard]] Result<uint64_t> CpuOn(uint64_t mpidr, uint64_t entry_address,
                                     uint64_t context) noexcept;

[[nodiscard]] uint32_t Version() noexcept;

}  // namespace og::psci

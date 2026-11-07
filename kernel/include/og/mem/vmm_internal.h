#pragma once

// Private VMM helpers shared by mapping and query units. Not a public API.

#include <cstdint>

#include "og/core/status.h"

namespace og::mem::detail {

constexpr uint64_t kTableEntries = 512;

[[nodiscard]] uint64_t Index(uint64_t virtual_address, unsigned level) noexcept;

[[nodiscard]] Result<uint64_t> TranslateInRoot(uint64_t* root,
                                               uint64_t virtual_address) noexcept;

[[nodiscard]] uint64_t* KernelRoot() noexcept;

[[nodiscard]] uint32_t UserSpaceL0Index() noexcept;

[[nodiscard]] uint32_t AllocateAsid() noexcept;

void ReleaseAsid(uint16_t asid) noexcept;

}  // namespace og::mem::detail

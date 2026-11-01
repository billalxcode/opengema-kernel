#pragma once

// Canonical owner of virtual memory mapping.
// Page tables live in identity mapped RAM, so a table descriptor address is
// also a valid pointer. This assumption is documented until a higher half
// layout exists.

#include <cstdint>

#include "og/arch/descriptor.h"
#include "og/bootinfo.h"
#include "og/status.h"

namespace og::mem {

// Builds the kernel identity map: everything below ram_base is device,
// RAM is normal and executable. Activates the tables before returning.
[[nodiscard]] Status VmmInit(const BootInfo& info) noexcept;

// Chooses 1 GiB, 2 MiB, or 4 KiB pages based on alignment and size.
[[nodiscard]] Status VmmMap(uint64_t virtual_address, uint64_t physical_address, uint64_t size,
                           arch::aarch64::MemoryKind kind, bool executable) noexcept;

[[nodiscard]] Status VmmUnmap(uint64_t virtual_address, uint64_t size) noexcept;

// Identity maps a device range, skipping pages that are already mapped.
// Used for PCIe ECAM and MMIO BARs that firmware placed above RAM.
[[nodiscard]] Status VmmMapDevice(uint64_t base, uint64_t size) noexcept;

[[nodiscard]] Result<uint64_t> VmmTranslate(uint64_t virtual_address) noexcept;

[[nodiscard]] bool VmmReady() noexcept;

}  // namespace og::mem

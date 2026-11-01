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
                           arch::aarch64::MemoryKind kind, bool executable,
                           bool user = false) noexcept;

[[nodiscard]] Status VmmUnmap(uint64_t virtual_address, uint64_t size) noexcept;

// Identity maps a device range, skipping pages that are already mapped.
// Used for PCIe ECAM and MMIO BARs that firmware placed above RAM.
[[nodiscard]] Status VmmMapDevice(uint64_t base, uint64_t size) noexcept;

// Rewrites attributes of an existing mapping at 4 KiB granularity, splitting
// larger blocks when needed. Used for W^X on the kernel image.
[[nodiscard]] Status VmmProtectRange(uint64_t base, uint64_t size, bool executable,
                                     bool writable) noexcept;

struct PageAttributes {
  bool executable_el1;
  bool executable_el0;
  bool writable;
  bool user;
};

// Reads back the permission bits of the page mapped at virtual_address.
[[nodiscard]] Result<PageAttributes> VmmQueryAttributes(uint64_t virtual_address) noexcept;

// Allocates and zeroes user accessible pages at virtual_address.
[[nodiscard]] Status VmmMapUser(uint64_t virtual_address, uint64_t size, bool executable) noexcept;

// Validated copies between kernel buffers and user mappings.
[[nodiscard]] Status VmmCopyIn(uint64_t user_address, void* destination, uint64_t size) noexcept;
[[nodiscard]] Status VmmCopyOut(uint64_t user_address, const void* source, uint64_t size) noexcept;
[[nodiscard]] bool VmmUserContains(uint64_t user_address, uint64_t size) noexcept;

// Address of the level 0 table, used when bringing up secondary cores.
[[nodiscard]] uint64_t VmmRootAddress() noexcept;

[[nodiscard]] Result<uint64_t> VmmTranslate(uint64_t virtual_address) noexcept;

[[nodiscard]] bool VmmReady() noexcept;

}  // namespace og::mem

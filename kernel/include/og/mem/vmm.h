#pragma once

// Canonical owner of virtual memory mapping.
// Page tables live in identity mapped RAM, so a table descriptor address is
// also a valid pointer. This assumption is documented until a higher half
// layout exists.

#include <cstdint>

#include "og/arch/descriptor.h"
#include "og/core/bootinfo.h"
#include "og/core/status.h"

namespace og::mem {

// User mappings live above the kernel identity map, in the level 0 slot
// after the device and RAM slots. Each process gets its own level 0 table
// there while the kernel entries stay shared.
constexpr uint64_t kUserSpaceBase = 1ULL << 40U;   // 1 TiB
constexpr uint64_t kUserSpaceSize = 1ULL << 39U;   // 512 GiB
constexpr uint32_t kMaxUserRegions = 16;
constexpr uint64_t kUserMemoryLimit = 32ULL * 1024U * 1024U;  // 32 MiB

struct UserRegion {
  uint64_t base;
  uint64_t size;
  bool lazy;
  bool executable;
  bool writable;
  bool shared;
};

struct AddressSpace {
  uint64_t root;
  uint64_t mmap_next;
  uint64_t user_bytes;
  uint16_t asid;
  uint32_t region_count;
  UserRegion regions[kMaxUserRegions];
};

// Builds a level 0 table that shares every kernel entry with the boot
// address space and owns the user slot. The caller keeps the object alive.
[[nodiscard]] Status AddressSpaceInit(AddressSpace& space) noexcept;

// Frees user pages, user page tables, and the level 0 table. Kernel entries
// are shared and stay untouched. The space must not be active.
[[nodiscard]] Status AddressSpaceDestroy(AddressSpace& space) noexcept;

// Clones user page tables and shares pages with the parent through COW.
// Page-table frames, FD entries, and child bookkeeping are duplicated.
[[nodiscard]] Status VmmCloneAddressSpace(const AddressSpace& parent,
                                          AddressSpace& child) noexcept;

// Installs the space on the current CPU. Null selects the kernel space.
void AddressSpaceActivate(const AddressSpace* space) noexcept;

// Builds the kernel identity map: everything below ram_base is device,
// RAM is normal and executable. Activates the tables before returning.
[[nodiscard]] Status VmmInit(const BootInfo& info) noexcept;

// Chooses 1 GiB, 2 MiB, or 4 KiB pages based on alignment and size.
[[nodiscard]] Status VmmMap(uint64_t virtual_address, uint64_t physical_address, uint64_t size,
                           arch::aarch64::MemoryKind kind, bool executable,
                           bool user = false, bool writable = true) noexcept;

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

// Allocates and zeroes user accessible pages in the given address space.
[[nodiscard]] Status VmmMapUser(AddressSpace& space, uint64_t virtual_address,
                                uint64_t size, bool executable,
                                bool writable = true,
                                bool shared = false) noexcept;

// Maps pages adjacent to an existing user region and extends that region.
// Used by stack growth so growth does not consume another region slot.
[[nodiscard]] Status VmmGrowUser(AddressSpace& space, uint64_t virtual_address,
                                 uint64_t size, bool executable,
                                 bool writable) noexcept;

// Reserves a lazy user region. Pages are allocated on first access.
[[nodiscard]] Status VmmMapUserLazy(AddressSpace& space,
                                    uint64_t virtual_address, uint64_t size,
                                    bool executable, bool writable) noexcept;

// Allocates the page containing virtual_address if the address is inside a
// lazy region.
[[nodiscard]] Status VmmFaultUserPage(AddressSpace& space,
                                      uint64_t virtual_address) noexcept;

[[nodiscard]] uint64_t VmmLazyFaultCount() noexcept;

// Unmaps a user region and frees its physical frames. The range must match
// a region created by VmmMapUser exactly.
[[nodiscard]] Status VmmUnmapUser(AddressSpace& space, uint64_t virtual_address,
                                  uint64_t size) noexcept;

// Invalidates the local TLB and, when configured, asks every other online
// CPU to do the same. Used after permission changes and unmaps.
using TlbShootdownHook = void (*)() noexcept;
void SetTlbShootdownHook(TlbShootdownHook hook) noexcept;
void TlbShootdown() noexcept;

// Rewrites user page permissions for code or data mappings.
[[nodiscard]] Status VmmProtectUser(AddressSpace& space, uint64_t base,
                                    uint64_t size, bool executable,
                                    bool writable) noexcept;

// Resolves a write fault on a COW page by copying it into a private frame.
[[nodiscard]] Status VmmHandleCow(AddressSpace& space,
                                  uint64_t virtual_address) noexcept;

// Validated copies between kernel buffers and user mappings. The copy walks
// the page tables, so it does not depend on which space is active.
[[nodiscard]] Status VmmCopyIn(const AddressSpace& space, uint64_t user_address,
                               void* destination, uint64_t size) noexcept;
[[nodiscard]] Status VmmCopyOut(const AddressSpace& space, uint64_t user_address,
                                const void* source, uint64_t size) noexcept;
[[nodiscard]] bool VmmUserContains(const AddressSpace& space, uint64_t user_address,
                                   uint64_t size) noexcept;

// Translation inside an explicit address space.
[[nodiscard]] Result<uint64_t> VmmTranslateIn(const AddressSpace& space,
                                              uint64_t virtual_address) noexcept;

// Address of the level 0 table, used when bringing up secondary cores.
[[nodiscard]] uint64_t VmmRootAddress() noexcept;

[[nodiscard]] Result<uint64_t> VmmTranslate(uint64_t virtual_address) noexcept;

[[nodiscard]] bool VmmReady() noexcept;

}  // namespace og::mem

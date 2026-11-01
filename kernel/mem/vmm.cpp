#include "og/vmm.h"

#include "memory.h"
#include "og/arch/cache.h"
#include "og/arch/descriptor.h"
#include "og/arch/tlb.h"
#include "og/mmu.h"
#include "og/pmm.h"

namespace og::mem {

namespace {

using arch::aarch64::BlockDescriptor;
using arch::aarch64::DescriptorAddress;
using arch::aarch64::DescriptorIsBlock;
using arch::aarch64::DescriptorIsTable;
using arch::aarch64::DescriptorValid;
using arch::aarch64::kBlockSize1G;
using arch::aarch64::kBlockSize2M;
using arch::aarch64::MemoryKind;
using arch::aarch64::PageDescriptor;
using arch::aarch64::TableDescriptor;

constexpr uint64_t kTableEntries = 512;

uint64_t* g_root = nullptr;

constexpr uint32_t kMaxUserRegions = 4;

struct UserRegion {
  uint64_t base;
  uint64_t size;
};

UserRegion g_user_regions[kMaxUserRegions];
uint32_t g_user_region_count = 0;

void TrackUserRegion(uint64_t base, uint64_t size) noexcept {
  if (g_user_region_count >= kMaxUserRegions) {
    return;
  }
  g_user_regions[g_user_region_count].base = base;
  g_user_regions[g_user_region_count].size = size;
  ++g_user_region_count;
}

[[nodiscard]] uint64_t Index(uint64_t virtual_address, unsigned level) noexcept {
  const unsigned shift = 39U - (level * 9U);
  return (virtual_address >> shift) & (kTableEntries - 1U);
}

[[nodiscard]] Result<uint64_t*> NextTable(uint64_t* table, uint64_t index) noexcept {
  const uint64_t descriptor = table[index];
  if (DescriptorValid(descriptor)) {
    if (!DescriptorIsTable(descriptor)) {
      return Result<uint64_t*>::Err(Status::kExists);
    }
    return Result<uint64_t*>::Ok(reinterpret_cast<uint64_t*>(DescriptorAddress(descriptor)));
  }

  const Result<uint64_t> frame = PmmAllocPage();
  if (frame.is_err()) {
    return Result<uint64_t*>::Err(frame.status());
  }
  const uint64_t address = frame.value();
  auto* fresh = reinterpret_cast<uint64_t*>(address);
  mem::Zero(fresh, kPageSize);
  table[index] = TableDescriptor(address);
  arch::aarch64::CleanInvalidateDcacheRange(address, address + kPageSize);
  return Result<uint64_t*>::Ok(fresh);
}

[[nodiscard]] Status MapChunk(uint64_t virtual_address, uint64_t physical_address, uint64_t size,
                             MemoryKind kind, bool executable, bool user) noexcept {
  uint64_t* table = g_root;
  for (unsigned level = 0; level < 3; ++level) {
    const uint64_t index = Index(virtual_address, level);
    if (level == 1 && size == kBlockSize1G) {
      if (DescriptorValid(table[index])) {
        return Status::kExists;
      }
      table[index] = BlockDescriptor(physical_address, kind, executable, user);
      return Status::kOk;
    }
    if (level == 2 && size == kBlockSize2M) {
      if (DescriptorValid(table[index])) {
        return Status::kExists;
      }
      table[index] = BlockDescriptor(physical_address, kind, executable, user);
      return Status::kOk;
    }
    const Result<uint64_t*> next = NextTable(table, index);
    if (next.is_err()) {
      return next.status();
    }
    table = next.value();
  }

  const uint64_t index = Index(virtual_address, 3);
  if (DescriptorValid(table[index])) {
    return Status::kExists;
  }
  table[index] = PageDescriptor(physical_address, kind, executable, user);
  return Status::kOk;
}

[[nodiscard]] Status UnmapChunk(uint64_t virtual_address, uint64_t size) noexcept {
  uint64_t* table = g_root;
  for (unsigned level = 0; level < 3; ++level) {
    const uint64_t index = Index(virtual_address, level);
    if (level == 1 && size == kBlockSize1G) {
      if (!DescriptorValid(table[index])) {
        return Status::kNotFound;
      }
      table[index] = 0;
      return Status::kOk;
    }
    if (level == 2 && size == kBlockSize2M) {
      if (!DescriptorValid(table[index])) {
        return Status::kNotFound;
      }
      table[index] = 0;
      return Status::kOk;
    }
    if (!DescriptorValid(table[index])) {
      return Status::kNotFound;
    }
    if (!DescriptorIsTable(table[index])) {
      return Status::kInvalidArg;
    }
    table = reinterpret_cast<uint64_t*>(DescriptorAddress(table[index]));
  }

  const uint64_t index = Index(virtual_address, 3);
  if (!DescriptorValid(table[index])) {
    return Status::kNotFound;
  }
  table[index] = 0;
  return Status::kOk;
}

[[nodiscard]] uint64_t ChunkSize(uint64_t virtual_address, uint64_t physical_address,
                                uint64_t remaining) noexcept {
  if (((virtual_address % kBlockSize1G) == 0) && ((physical_address % kBlockSize1G) == 0) &&
      remaining >= kBlockSize1G) {
    return kBlockSize1G;
  }
  if (((virtual_address % kBlockSize2M) == 0) && ((physical_address % kBlockSize2M) == 0) &&
      remaining >= kBlockSize2M) {
    return kBlockSize2M;
  }
  return kPageSize;
}

[[nodiscard]] Status MapRange(uint64_t virtual_address, uint64_t physical_address, uint64_t size,
                             MemoryKind kind, bool executable, bool user) noexcept {
  uint64_t offset = 0;
  while (offset < size) {
    const uint64_t current_virtual = virtual_address + offset;
    const uint64_t current_physical = physical_address + offset;
    const uint64_t remaining = size - offset;
    const uint64_t step = ChunkSize(current_virtual, current_physical, remaining);
    const Status status =
      MapChunk(current_virtual, current_physical, step, kind, executable, user);
    if (IsErr(status)) {
      return status;
    }
    arch::aarch64::InvalidateTlbVaLocal(current_virtual);
    offset += step;
  }
  arch::aarch64::InvalidateTlbSync();
  return Status::kOk;
}

[[nodiscard]] Status UnmapRange(uint64_t virtual_address, uint64_t size) noexcept {
  uint64_t offset = 0;
  while (offset < size) {
    const uint64_t current = virtual_address + offset;
    const uint64_t remaining = size - offset;
    uint64_t step = kPageSize;
    if (((current % kBlockSize1G) == 0) && remaining >= kBlockSize1G) {
      step = kBlockSize1G;
    } else if (((current % kBlockSize2M) == 0) && remaining >= kBlockSize2M) {
      step = kBlockSize2M;
    }
    const Status status = UnmapChunk(current, step);
    if (IsErr(status)) {
      return status;
    }
    arch::aarch64::InvalidateTlbVaLocal(current);
    offset += step;
  }
  arch::aarch64::InvalidateTlbSync();
  return Status::kOk;
}

}  // namespace

Status VmmInit(const BootInfo& info) noexcept {
  if (g_root != nullptr) {
    return Status::kOk;
  }
  if (info.ram_base == 0) {
    return Status::kInvalidArg;
  }
  if (info.ram_size == 0) {
    return Status::kInvalidArg;
  }
  if ((info.ram_base % kBlockSize1G) != 0) {
    return Status::kUnaligned;
  }

  const Result<uint64_t> root = PmmAllocPage();
  if (root.is_err()) {
    return root.status();
  }
  auto* tables = reinterpret_cast<uint64_t*>(root.value());
  mem::Zero(tables, kPageSize);
  g_root = tables;

  // Device space below RAM, then RAM itself.
  Status status = MapRange(0, 0, info.ram_base, MemoryKind::kDevice, false, false);
  if (IsErr(status)) {
    return status;
  }
  status = MapRange(info.ram_base, info.ram_base, info.ram_size, MemoryKind::kNormal, true, false);
  if (IsErr(status)) {
    return status;
  }

  return arch::aarch64::ActivateAddressSpace(root.value());
}

Status VmmMap(uint64_t virtual_address, uint64_t physical_address, uint64_t size,
              MemoryKind kind, bool executable, bool user) noexcept {
  if (g_root == nullptr) {
    return Status::kNotInit;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  if ((virtual_address % kPageSize) != 0) {
    return Status::kUnaligned;
  }
  if ((physical_address % kPageSize) != 0) {
    return Status::kUnaligned;
  }
  if ((size % kPageSize) != 0) {
    return Status::kInvalidArg;
  }
  if ((virtual_address + size) < virtual_address) {
    return Status::kInvalidArg;
  }
  if ((physical_address + size) < physical_address) {
    return Status::kInvalidArg;
  }
  return MapRange(virtual_address, physical_address, size, kind, executable, user);
}

Status VmmUnmap(uint64_t virtual_address, uint64_t size) noexcept {
  if (g_root == nullptr) {
    return Status::kNotInit;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  if ((virtual_address % kPageSize) != 0) {
    return Status::kUnaligned;
  }
  if ((size % kPageSize) != 0) {
    return Status::kInvalidArg;
  }
  return UnmapRange(virtual_address, size);
}

Status VmmMapDevice(uint64_t base, uint64_t size) noexcept {
  if (g_root == nullptr) {
    return Status::kNotInit;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  if ((base + size) < base) {
    return Status::kInvalidArg;
  }

  const uint64_t start = base & ~(kPageSize - 1U);
  const uint64_t end = (base + size + kPageSize - 1U) & ~(kPageSize - 1U);

  // Fast path: both ends already mapped, assume the range is covered.
  const Result<uint64_t> start_mapped = VmmTranslate(start);
  const Result<uint64_t> end_mapped = VmmTranslate(end - kPageSize);
  if (start_mapped.is_ok() && end_mapped.is_ok()) {
    return Status::kOk;
  }

  // Map whole range so the 1 GiB and 2 MiB paths pick the biggest block.
  const Status status = VmmMap(start, start, end - start, MemoryKind::kDevice, false, false);
  if (IsOk(status)) {
    return Status::kOk;
  }
  if (status != Status::kExists) {
    return status;
  }

  // Partial overlap with existing mappings: fill page by page.
  for (uint64_t address = start; address < end; address += kPageSize) {
    const Result<uint64_t> translated = VmmTranslate(address);
    if (translated.is_ok()) {
      continue;
    }
    const Status page_status = VmmMap(address, address, kPageSize, MemoryKind::kDevice, false, false);
    if (IsErr(page_status)) {
      return page_status;
    }
  }
  return Status::kOk;
}

Result<uint64_t> VmmTranslate(uint64_t virtual_address) noexcept {  if (g_root == nullptr) {
    return Result<uint64_t>::Err(Status::kNotInit);
  }

  uint64_t* table = g_root;
  for (unsigned level = 0; level < 3; ++level) {
    const uint64_t descriptor = table[Index(virtual_address, level)];
    if (!DescriptorValid(descriptor)) {
      return Result<uint64_t>::Err(Status::kNotFound);
    }
    if (level >= 1 && DescriptorIsBlock(descriptor)) {
      const uint64_t block = (level == 1) ? kBlockSize1G : kBlockSize2M;
      return Result<uint64_t>::Ok(DescriptorAddress(descriptor) |
                                  (virtual_address & (block - 1U)));
    }
    if (!DescriptorIsTable(descriptor)) {
      return Result<uint64_t>::Err(Status::kNotFound);
    }
    table = reinterpret_cast<uint64_t*>(DescriptorAddress(descriptor));
  }

  const uint64_t descriptor = table[Index(virtual_address, 3)];
  if (!DescriptorValid(descriptor)) {
    return Result<uint64_t>::Err(Status::kNotFound);
  }
  return Result<uint64_t>::Ok(DescriptorAddress(descriptor) |
                              (virtual_address & (kPageSize - 1U)));
}

Status VmmMapUser(uint64_t virtual_address, uint64_t size, bool executable) noexcept {
  if (g_root == nullptr) {
    return Status::kNotInit;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  if ((virtual_address % kPageSize) != 0) {
    return Status::kUnaligned;
  }
  const uint64_t aligned = (size + kPageSize - 1U) & ~(kPageSize - 1U);
  if ((virtual_address + aligned) < virtual_address) {
    return Status::kInvalidArg;
  }

  for (uint64_t offset = 0; offset < aligned; offset += kPageSize) {
    const Result<uint64_t> frame = PmmAllocPage();
    if (frame.is_err()) {
      return frame.status();
    }
    mem::Zero(reinterpret_cast<void*>(frame.value()), kPageSize);
    const Status status = VmmMap(virtual_address + offset, frame.value(), kPageSize,
                                 MemoryKind::kNormal, executable, true);
    if (IsErr(status)) {
      (void)PmmFreePage(frame.value());
      return status;
    }
  }

  TrackUserRegion(virtual_address, aligned);
  return Status::kOk;
}

bool VmmUserContains(uint64_t user_address, uint64_t size) noexcept {
  if (size == 0) {
    return false;
  }
  const uint64_t end = user_address + size;
  if (end < user_address) {
    return false;
  }
  for (uint32_t index = 0; index < g_user_region_count; ++index) {
    const UserRegion& region = g_user_regions[index];
    const uint64_t region_end = region.base + region.size;
    if (user_address >= region.base && end <= region_end) {
      return true;
    }
  }
  return false;
}

Status VmmCopyIn(uint64_t user_address, void* destination, uint64_t size) noexcept {
  if (destination == nullptr) {
    return Status::kInvalidArg;
  }
  if (!VmmUserContains(user_address, size)) {
    return Status::kInvalidArg;
  }
  mem::Copy(destination, reinterpret_cast<const void*>(user_address), size);
  return Status::kOk;
}

Status VmmCopyOut(uint64_t user_address, const void* source, uint64_t size) noexcept {
  if (source == nullptr) {
    return Status::kInvalidArg;
  }
  if (!VmmUserContains(user_address, size)) {
    return Status::kInvalidArg;
  }
  mem::Copy(reinterpret_cast<void*>(user_address), source, size);
  return Status::kOk;
}

bool VmmReady() noexcept {
  return g_root != nullptr;
}

uint64_t VmmRootAddress() noexcept {
  return reinterpret_cast<uint64_t>(g_root);
}

}  // namespace og::mem

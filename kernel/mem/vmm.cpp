#include "og/mem/vmm.h"

#include "memory.h"
#include "og/arch/cache.h"
#include "og/arch/cpu.h"
#include "og/arch/descriptor.h"
#include "og/arch/mmu.h"
#include "og/arch/tlb.h"
#include "og/mem/pmm.h"

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
constexpr uint32_t kUserSpaceL0Index =
  static_cast<uint32_t>(kUserSpaceBase >> 39U);
constexpr uint32_t kMaxCpus = 4;

uint64_t* g_root = nullptr;

struct ActiveSpace {
  uint64_t root;
  uint16_t asid;
  bool valid;
};

ActiveSpace g_active_spaces[kMaxCpus];
uint32_t g_next_asid = 1;

void TrackUserRegion(AddressSpace& space, uint64_t base, uint64_t size) noexcept {
  if (space.region_count >= kMaxUserRegions) {
    return;
  }
  space.regions[space.region_count].base = base;
  space.regions[space.region_count].size = size;
  ++space.region_count;
}

void DropUserRegion(AddressSpace& space, uint32_t index) noexcept {
  for (uint32_t current = index; (current + 1U) < space.region_count; ++current) {
    space.regions[current] = space.regions[current + 1U];
  }
  if (space.region_count > 0U) {
    --space.region_count;
  }
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

// Returns the table below table[index]. If a block descriptor is present it
// is split into the next level, preserving its attributes.
[[nodiscard]] Result<uint64_t*> EnsureTable(uint64_t* table, uint64_t index,
                                            unsigned level) noexcept {
  const uint64_t descriptor = table[index];
  if (!DescriptorValid(descriptor)) {
    return Result<uint64_t*>::Err(Status::kNotFound);
  }
  if (DescriptorIsTable(descriptor)) {
    return Result<uint64_t*>::Ok(reinterpret_cast<uint64_t*>(DescriptorAddress(descriptor)));
  }

  const Result<uint64_t> frame = PmmAllocPage();
  if (frame.is_err()) {
    return Result<uint64_t*>::Err(frame.status());
  }
  auto* child = reinterpret_cast<uint64_t*>(frame.value());
  mem::Zero(child, kPageSize);

  const uint64_t block_size = (level == 1U) ? kBlockSize1G : kBlockSize2M;
  const uint64_t child_size = block_size / kTableEntries;
  const uint64_t base_address = DescriptorAddress(descriptor);
  const uint64_t low_attributes = descriptor & 0xFFFULL;
  const uint64_t high_attributes = descriptor & (arch::aarch64::kPxn | arch::aarch64::kUxn);
  const uint64_t child_type =
    (level == 1U) ? arch::aarch64::kDescriptorBlock : arch::aarch64::kDescriptorPage;

  for (uint64_t entry = 0; entry < kTableEntries; ++entry) {
    const uint64_t address = base_address + (entry * child_size);
    child[entry] = address | (low_attributes & ~3ULL) | high_attributes | child_type;
  }

  table[index] = TableDescriptor(frame.value());
  arch::aarch64::CleanInvalidateDcacheRange(frame.value(), frame.value() + kPageSize);
  return Result<uint64_t*>::Ok(child);
}

[[nodiscard]] Status MapChunk(uint64_t* root, uint64_t virtual_address,
                             uint64_t physical_address, uint64_t size,
                             MemoryKind kind, bool executable, bool user) noexcept {
  uint64_t* table = root;
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

[[nodiscard]] Status UnmapChunk(uint64_t* root, uint64_t virtual_address,
                               uint64_t size) noexcept {
  uint64_t* table = root;
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

[[nodiscard]] Status MapRange(uint64_t* root, uint64_t virtual_address,
                             uint64_t physical_address, uint64_t size,
                             MemoryKind kind, bool executable, bool user) noexcept {
  uint64_t offset = 0;
  while (offset < size) {
    const uint64_t current_virtual = virtual_address + offset;
    const uint64_t current_physical = physical_address + offset;
    const uint64_t remaining = size - offset;
    const uint64_t step = ChunkSize(current_virtual, current_physical, remaining);
    const Status status =
      MapChunk(root, current_virtual, current_physical, step, kind, executable, user);
    if (IsErr(status)) {
      return status;
    }
    arch::aarch64::InvalidateTlbVaLocal(current_virtual);
    offset += step;
  }
  arch::aarch64::InvalidateTlbSync();
  return Status::kOk;
}

[[nodiscard]] Status UnmapRange(uint64_t* root, uint64_t virtual_address,
                               uint64_t size) noexcept {
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
    const Status status = UnmapChunk(root, current, step);
    if (IsErr(status)) {
      return status;
    }
    arch::aarch64::InvalidateTlbVaLocal(current);
    offset += step;
  }
  arch::aarch64::InvalidateTlbSync();
  return Status::kOk;
}

[[nodiscard]] Result<uint64_t> TranslateInRoot(uint64_t* root,
                                               uint64_t virtual_address) noexcept {
  if (root == nullptr) {
    return Result<uint64_t>::Err(Status::kNotInit);
  }

  uint64_t* table = root;
  for (unsigned level = 0; level < 3; ++level) {
    const uint64_t descriptor = table[Index(virtual_address, level)];
    if (!DescriptorValid(descriptor)) {
      return Result<uint64_t>::Err(Status::kNotFound);
    }
    if (level >= 1U && DescriptorIsBlock(descriptor)) {
      const uint64_t block = (level == 1U) ? kBlockSize1G : kBlockSize2M;
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
  Status status = MapRange(g_root, 0, 0, info.ram_base, MemoryKind::kDevice, false, false);
  if (IsErr(status)) {
    return status;
  }
  status = MapRange(g_root, info.ram_base, info.ram_base, info.ram_size,
                    MemoryKind::kNormal, false, false);
  if (IsErr(status)) {
    return status;
  }

  // W^X: only the executable section of the kernel image stays executable
  // and read only. Everything else in RAM is no-execute.
  if (info.text_size != 0 && info.text_base != 0) {
    status = VmmProtectRange(info.text_base, info.text_size, true, false);
    if (IsErr(status)) {
      return status;
    }
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
  return MapRange(g_root, virtual_address, physical_address, size, kind, executable, user);
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
  return UnmapRange(g_root, virtual_address, size);
}

Status VmmProtectRange(uint64_t base, uint64_t size, bool executable,
                       bool writable) noexcept {
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

  for (uint64_t address = start; address < end; address += kPageSize) {
    const uint64_t root_entry = g_root[Index(address, 0)];
    if (!DescriptorValid(root_entry) || !DescriptorIsTable(root_entry)) {
      return Status::kNotFound;
    }
    auto* level1 = reinterpret_cast<uint64_t*>(DescriptorAddress(root_entry));

    const Result<uint64_t*> level2 = EnsureTable(level1, Index(address, 1), 1);
    if (level2.is_err()) {
      return level2.status();
    }
    const Result<uint64_t*> level3 = EnsureTable(level2.value(), Index(address, 2), 2);
    if (level3.is_err()) {
      return level3.status();
    }

    uint64_t& entry = level3.value()[Index(address, 3)];
    if (!DescriptorValid(entry)) {
      return Status::kNotFound;
    }
    entry &= ~(arch::aarch64::kPxn | arch::aarch64::kUxn | (3ULL << 6U));
    entry |= arch::aarch64::kUxn;
    if (!executable) {
      entry |= arch::aarch64::kPxn;
    }
    if (!writable) {
      entry |= arch::aarch64::kApEl1Ro;
    }
    arch::aarch64::InvalidateTlbVaLocal(address);
  }

  arch::aarch64::InvalidateTlbSync();
  return Status::kOk;
}

Status VmmMapDevice(uint64_t base, uint64_t size) noexcept {  if (g_root == nullptr) {
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

namespace {

[[nodiscard]] PageAttributes DecodeAttributes(uint64_t descriptor) noexcept {
  PageAttributes attributes{};
  attributes.executable_el1 = (descriptor & arch::aarch64::kPxn) == 0;
  attributes.executable_el0 = (descriptor & arch::aarch64::kUxn) == 0;
  const uint64_t access = descriptor & (3ULL << 6U);
  attributes.writable = access != arch::aarch64::kApEl1Ro;
  attributes.user = access == arch::aarch64::kApUserRw;
  return attributes;
}

}  // namespace

Result<PageAttributes> VmmQueryAttributes(uint64_t virtual_address) noexcept {
  if (g_root == nullptr) {
    return Result<PageAttributes>::Err(Status::kNotInit);
  }

  uint64_t* table = g_root;
  for (unsigned level = 0; level < 3; ++level) {
    const uint64_t descriptor = table[Index(virtual_address, level)];
    if (!DescriptorValid(descriptor)) {
      return Result<PageAttributes>::Err(Status::kNotFound);
    }
    if (level >= 1U && DescriptorIsBlock(descriptor)) {
      return Result<PageAttributes>::Ok(DecodeAttributes(descriptor));
    }
    if (!DescriptorIsTable(descriptor)) {
      return Result<PageAttributes>::Err(Status::kNotFound);
    }
    table = reinterpret_cast<uint64_t*>(DescriptorAddress(descriptor));
  }

  const uint64_t descriptor = table[Index(virtual_address, 3)];
  if (!DescriptorValid(descriptor)) {
    return Result<PageAttributes>::Err(Status::kNotFound);
  }
  return Result<PageAttributes>::Ok(DecodeAttributes(descriptor));
}

Result<uint64_t> VmmTranslate(uint64_t virtual_address) noexcept {
  return TranslateInRoot(g_root, virtual_address);
}

Result<uint64_t> VmmTranslateIn(const AddressSpace& space,
                                uint64_t virtual_address) noexcept {
  return TranslateInRoot(reinterpret_cast<uint64_t*>(space.root), virtual_address);
}

Status VmmMapUser(AddressSpace& space, uint64_t virtual_address, uint64_t size,
                  bool executable) noexcept {
  if (g_root == nullptr || space.root == 0) {
    return Status::kNotInit;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  if ((virtual_address % kPageSize) != 0) {
    return Status::kUnaligned;
  }
  if (size > kUserSpaceSize) {
    return Status::kInvalidArg;
  }
  const uint64_t aligned = (size + kPageSize - 1U) & ~(kPageSize - 1U);
  if (virtual_address < kUserSpaceBase) {
    return Status::kInvalidArg;
  }
  if (aligned > kUserSpaceSize) {
    return Status::kInvalidArg;
  }
  const uint64_t offset_in_space = virtual_address - kUserSpaceBase;
  if (offset_in_space > (kUserSpaceSize - aligned)) {
    return Status::kInvalidArg;
  }

  auto* root = reinterpret_cast<uint64_t*>(space.root);
  for (uint64_t offset = 0; offset < aligned; offset += kPageSize) {
    const Result<uint64_t> frame = PmmAllocPage();
    if (frame.is_err()) {
      return frame.status();
    }
    mem::Zero(reinterpret_cast<void*>(frame.value()), kPageSize);
    const Status status = MapChunk(root, virtual_address + offset, frame.value(),
                                   kPageSize, MemoryKind::kNormal, executable, true);
    if (IsErr(status)) {
      (void)PmmFreePage(frame.value());
      return status;
    }
  }

  arch::aarch64::InvalidateTlbAllLocal();
  TrackUserRegion(space, virtual_address, aligned);
  return Status::kOk;
}

Status VmmUnmapUser(AddressSpace& space, uint64_t virtual_address,
                    uint64_t size) noexcept {
  if (g_root == nullptr || space.root == 0) {
    return Status::kNotInit;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  if ((virtual_address % kPageSize) != 0) {
    return Status::kUnaligned;
  }
  if (size > kUserSpaceSize) {
    return Status::kInvalidArg;
  }
  const uint64_t aligned = (size + kPageSize - 1U) & ~(kPageSize - 1U);

  uint32_t region_index = 0;
  bool found = false;
  for (uint32_t index = 0; index < space.region_count; ++index) {
    if (space.regions[index].base == virtual_address &&
        space.regions[index].size == aligned) {
      region_index = index;
      found = true;
      break;
    }
  }
  if (!found) {
    return Status::kInvalidArg;
  }

  auto* root = reinterpret_cast<uint64_t*>(space.root);
  for (uint64_t offset = 0; offset < aligned; offset += kPageSize) {
    const uint64_t address = virtual_address + offset;

    const uint64_t root_entry = root[Index(address, 0)];
    if (!DescriptorValid(root_entry) || !DescriptorIsTable(root_entry)) {
      return Status::kNotFound;
    }
    auto* level1 = reinterpret_cast<uint64_t*>(DescriptorAddress(root_entry));

    const uint64_t level1_entry = level1[Index(address, 1)];
    if (!DescriptorValid(level1_entry) || !DescriptorIsTable(level1_entry)) {
      return Status::kNotFound;
    }
    auto* level2 = reinterpret_cast<uint64_t*>(DescriptorAddress(level1_entry));

    const uint64_t level2_entry = level2[Index(address, 2)];
    if (!DescriptorValid(level2_entry) || !DescriptorIsTable(level2_entry)) {
      return Status::kNotFound;
    }
    auto* level3 = reinterpret_cast<uint64_t*>(DescriptorAddress(level2_entry));

    uint64_t& entry = level3[Index(address, 3)];
    if (!DescriptorValid(entry)) {
      return Status::kNotFound;
    }
    const uint64_t physical = DescriptorAddress(entry);
    entry = 0;
    (void)PmmFreePage(physical);
  }
  arch::aarch64::InvalidateTlbAllLocal();

  DropUserRegion(space, region_index);
  return Status::kOk;
}

bool VmmUserContains(const AddressSpace& space, uint64_t user_address,
                     uint64_t size) noexcept {
  if (size == 0) {
    return false;
  }
  const uint64_t end = user_address + size;
  if (end < user_address) {
    return false;
  }
  for (uint32_t index = 0; index < space.region_count; ++index) {
    const UserRegion& region = space.regions[index];
    const uint64_t region_end = region.base + region.size;
    if (user_address >= region.base && end <= region_end) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] Status CopyUser(const AddressSpace& space, uint64_t user_address,
                              void* kernel_address, uint64_t size,
                              bool to_user) noexcept {
  uint64_t offset = 0;
  while (offset < size) {
    const uint64_t current = user_address + offset;
    const Result<uint64_t> physical = VmmTranslateIn(space, current);
    if (physical.is_err()) {
      return physical.status();
    }
    const uint64_t page_remaining = kPageSize - (current % kPageSize);
    const uint64_t chunk =
      (size - offset) < page_remaining ? (size - offset) : page_remaining;
    auto* kernel = static_cast<uint8_t*>(kernel_address) + offset;
    auto* device = reinterpret_cast<uint8_t*>(physical.value());
    if (to_user) {
      mem::Copy(device, kernel, chunk);
    } else {
      mem::Copy(kernel, device, chunk);
    }
    offset += chunk;
  }
  return Status::kOk;
}

Status VmmCopyIn(const AddressSpace& space, uint64_t user_address,
                 void* destination, uint64_t size) noexcept {
  if (destination == nullptr) {
    return Status::kInvalidArg;
  }
  if (!VmmUserContains(space, user_address, size)) {
    return Status::kInvalidArg;
  }
  return CopyUser(space, user_address, destination, size, false);
}

Status VmmCopyOut(const AddressSpace& space, uint64_t user_address,
                  const void* source, uint64_t size) noexcept {
  if (source == nullptr) {
    return Status::kInvalidArg;
  }
  if (!VmmUserContains(space, user_address, size)) {
    return Status::kInvalidArg;
  }
  return CopyUser(space, user_address, const_cast<void*>(source), size, true);
}

bool VmmReady() noexcept {
  return g_root != nullptr;
}

uint64_t VmmRootAddress() noexcept {
  return reinterpret_cast<uint64_t>(g_root);
}

Status AddressSpaceInit(AddressSpace& space) noexcept {
  if (g_root == nullptr) {
    return Status::kNotInit;
  }
  if (space.root != 0) {
    return Status::kExists;
  }
  if (g_next_asid > 255U) {
    return Status::kNoMem;
  }

  const Result<uint64_t> root = PmmAllocPage();
  if (root.is_err()) {
    return root.status();
  }
  mem::Copy(reinterpret_cast<void*>(root.value()), g_root, kPageSize);
  auto* table = reinterpret_cast<uint64_t*>(root.value());
  table[kUserSpaceL0Index] = 0;
  arch::aarch64::CleanInvalidateDcacheRange(root.value(), root.value() + kPageSize);

  mem::Zero(space.regions, sizeof(space.regions));
  space.root = root.value();
  space.mmap_next = kUserSpaceBase + 0x1000000ULL;
  space.asid = static_cast<uint16_t>(g_next_asid);
  space.region_count = 0;
  ++g_next_asid;
  return Status::kOk;
}

Status AddressSpaceDestroy(AddressSpace& space) noexcept {
  if (space.root == 0) {
    return Status::kNotInit;
  }

  auto* level0 = reinterpret_cast<uint64_t*>(space.root);
  const uint64_t level1_descriptor = level0[kUserSpaceL0Index];
  if (DescriptorValid(level1_descriptor) && DescriptorIsTable(level1_descriptor)) {
    auto* level1 = reinterpret_cast<uint64_t*>(DescriptorAddress(level1_descriptor));
    for (uint64_t index1 = 0; index1 < kTableEntries; ++index1) {
      const uint64_t level2_descriptor = level1[index1];
      if (!DescriptorValid(level2_descriptor) || !DescriptorIsTable(level2_descriptor)) {
        continue;
      }
      auto* level2 = reinterpret_cast<uint64_t*>(DescriptorAddress(level2_descriptor));
      for (uint64_t index2 = 0; index2 < kTableEntries; ++index2) {
        const uint64_t level3_descriptor = level2[index2];
        if (!DescriptorValid(level3_descriptor) || !DescriptorIsTable(level3_descriptor)) {
          continue;
        }
        auto* level3 = reinterpret_cast<uint64_t*>(DescriptorAddress(level3_descriptor));
        for (uint64_t index3 = 0; index3 < kTableEntries; ++index3) {
          const uint64_t page = level3[index3];
          if (DescriptorValid(page)) {
            (void)PmmFreePage(DescriptorAddress(page));
          }
        }
        (void)PmmFreePage(reinterpret_cast<uint64_t>(level3));
      }
      (void)PmmFreePage(reinterpret_cast<uint64_t>(level2));
    }
    (void)PmmFreePage(reinterpret_cast<uint64_t>(level1));
    level0[kUserSpaceL0Index] = 0;
  }

  (void)PmmFreePage(space.root);
  mem::Zero(&space, sizeof(space));
  arch::aarch64::InvalidateTlbAllLocal();
  return Status::kOk;
}

void AddressSpaceActivate(const AddressSpace* space) noexcept {
  if (g_root == nullptr) {
    return;
  }
  const uint64_t root = space == nullptr ? reinterpret_cast<uint64_t>(g_root) : space->root;
  const uint16_t asid = space == nullptr ? 0U : space->asid;
  const uint32_t cpu = static_cast<uint32_t>(arch::aarch64::CpuId() & (kMaxCpus - 1U));

  ActiveSpace& active = g_active_spaces[cpu];
  if (active.valid && active.root == root && active.asid == asid) {
    return;
  }
  arch::aarch64::SwitchAddressSpace(root, asid);
  active.root = root;
  active.asid = asid;
  active.valid = true;
}

}  // namespace og::mem

#include "og/mem/vmm.h"

#include "memory.h"
#include "og/arch/cache.h"
#include "og/arch/descriptor.h"
#include "og/arch/tlb.h"
#include "og/core/random.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm_internal.h"

namespace og::mem {

namespace {

constexpr uint64_t kMmapBase = kUserSpaceBase + 0x1000000ULL;
constexpr uint64_t kMmapRandomRange = 16ULL * 1024U * 1024U;
constexpr uint64_t kMmapRandomSlots = kMmapRandomRange / kPageSize;

[[nodiscard]] Status CloneTable(uint64_t* source, uint64_t* destination,
                               unsigned level) noexcept {
  for (uint64_t index = 0; index < detail::kTableEntries; ++index) {
    const uint64_t descriptor = source[index];
    if (!arch::aarch64::DescriptorValid(descriptor)) {
      continue;
    }
    if (level < 3U) {
      if (!arch::aarch64::DescriptorIsTable(descriptor)) {
        return Status::kUnsupported;
      }
      const Result<uint64_t> frame = PmmAllocPage();
      if (frame.is_err()) {
        return frame.status();
      }
      auto* table = reinterpret_cast<uint64_t*>(frame.value());
      mem::Zero(table, kPageSize);
      destination[index] = arch::aarch64::TableDescriptor(frame.value());
      arch::aarch64::CleanInvalidateDcacheRange(
        frame.value(), frame.value() + kPageSize);
      const Status status = CloneTable(
        reinterpret_cast<uint64_t*>(
          arch::aarch64::DescriptorAddress(descriptor)),
        table, level + 1U);
      if (IsErr(status)) {
        return status;
      }
      continue;
    }

    const uint64_t physical = arch::aarch64::DescriptorAddress(descriptor);
    const arch::aarch64::MemoryKind kind =
      ((descriptor >> arch::aarch64::kAttrIndexShift) & 1ULL) != 0ULL
        ? arch::aarch64::MemoryKind::kDevice
        : arch::aarch64::MemoryKind::kNormal;
    const bool executable = (descriptor & arch::aarch64::kUxn) == 0U;
    // Share every user page COW: even a read-only code page can become
    // writable through mprotect later.
    const Status retained = PmmRetainPage(physical);
    if (IsErr(retained)) {
      return retained;
    }
    destination[index] = arch::aarch64::PageDescriptor(
      physical, kind, executable, true, false);
    destination[index] |= arch::aarch64::kPteCow;
    source[index] = (source[index] &
                     ~(3ULL << arch::aarch64::kApShift)) |
                    arch::aarch64::kApUserRo | arch::aarch64::kPteCow;
  }
  arch::aarch64::CleanInvalidateDcacheRange(
    reinterpret_cast<uint64_t>(source),
    reinterpret_cast<uint64_t>(source) + kPageSize);
  return Status::kOk;
}

}  // namespace

Status AddressSpaceInit(AddressSpace& space) noexcept {
  uint64_t* kernel_root = detail::KernelRoot();
  if (kernel_root == nullptr) {
    return Status::kNotInit;
  }
  if (space.root != 0) {
    return Status::kExists;
  }
  const uint32_t asid = detail::AllocateAsid();
  if (asid == 0U || asid > 255U) {
    return Status::kNoMem;
  }

  const Result<uint64_t> root = PmmAllocPage();
  if (root.is_err()) {
    return root.status();
  }
  mem::Copy(reinterpret_cast<void*>(root.value()), kernel_root,
            arch::aarch64::kPageSize);
  auto* table = reinterpret_cast<uint64_t*>(root.value());
  table[detail::UserSpaceL0Index()] = 0;
  arch::aarch64::CleanInvalidateDcacheRange(
    root.value(), root.value() + arch::aarch64::kPageSize);

  mem::Zero(space.regions, sizeof(space.regions));
  space.root = root.value();
  space.mmap_next =
    kMmapBase + ((core::Random64() % kMmapRandomSlots) * kPageSize);
  space.user_bytes = 0;
  space.asid = static_cast<uint16_t>(asid);
  space.region_count = 0;
  return Status::kOk;
}

Status AddressSpaceDestroy(AddressSpace& space) noexcept {
  if (space.root == 0) {
    return Status::kNotInit;
  }

  auto* level0 = reinterpret_cast<uint64_t*>(space.root);
  const uint64_t level1_descriptor =
    level0[detail::UserSpaceL0Index()];
  if (arch::aarch64::DescriptorValid(level1_descriptor) &&
      arch::aarch64::DescriptorIsTable(level1_descriptor)) {
    auto* level1 = reinterpret_cast<uint64_t*>(
      arch::aarch64::DescriptorAddress(level1_descriptor));
    for (uint64_t index1 = 0; index1 < detail::kTableEntries; ++index1) {
      const uint64_t level2_descriptor = level1[index1];
      if (!arch::aarch64::DescriptorValid(level2_descriptor) ||
          !arch::aarch64::DescriptorIsTable(level2_descriptor)) {
        continue;
      }
      auto* level2 = reinterpret_cast<uint64_t*>(
        arch::aarch64::DescriptorAddress(level2_descriptor));
      for (uint64_t index2 = 0; index2 < detail::kTableEntries; ++index2) {
        const uint64_t level3_descriptor = level2[index2];
        if (!arch::aarch64::DescriptorValid(level3_descriptor) ||
            !arch::aarch64::DescriptorIsTable(level3_descriptor)) {
          continue;
        }
        auto* level3 = reinterpret_cast<uint64_t*>(
          arch::aarch64::DescriptorAddress(level3_descriptor));
        for (uint64_t index3 = 0; index3 < detail::kTableEntries; ++index3) {
          const uint64_t page = level3[index3];
          if (arch::aarch64::DescriptorValid(page)) {
            (void)PmmFreePage(arch::aarch64::DescriptorAddress(page));
          }
        }
        (void)PmmFreePage(reinterpret_cast<uint64_t>(level3));
      }
      (void)PmmFreePage(reinterpret_cast<uint64_t>(level2));
    }
    (void)PmmFreePage(reinterpret_cast<uint64_t>(level1));
    level0[detail::UserSpaceL0Index()] = 0;
  }

  (void)PmmFreePage(space.root);
  mem::Zero(&space, sizeof(space));
  arch::aarch64::InvalidateTlbAllLocal();
  return Status::kOk;
}

Status VmmCloneAddressSpace(const AddressSpace& parent,
                            AddressSpace& child) noexcept {
  Status status = AddressSpaceInit(child);
  if (IsErr(status)) {
    return status;
  }

  child.mmap_next = parent.mmap_next;
  child.user_bytes = parent.user_bytes;
  child.region_count = parent.region_count;
  mem::Copy(child.regions, parent.regions, sizeof(child.regions));

  auto* parent_root = reinterpret_cast<uint64_t*>(parent.root);
  auto* child_root = reinterpret_cast<uint64_t*>(child.root);
  const uint64_t user_index = detail::UserSpaceL0Index();
  const uint64_t parent_descriptor = parent_root[user_index];
  if (!arch::aarch64::DescriptorValid(parent_descriptor)) {
    return Status::kOk;
  }
  if (!arch::aarch64::DescriptorIsTable(parent_descriptor)) {
    (void)AddressSpaceDestroy(child);
    return Status::kUnsupported;
  }

  const Result<uint64_t> frame = PmmAllocPage();
  if (frame.is_err()) {
    (void)AddressSpaceDestroy(child);
    return frame.status();
  }
  auto* child_level1 = reinterpret_cast<uint64_t*>(frame.value());
  mem::Zero(child_level1, kPageSize);
  child_root[user_index] = arch::aarch64::TableDescriptor(frame.value());
  arch::aarch64::CleanInvalidateDcacheRange(frame.value(),
                                            frame.value() + kPageSize);

  status = CloneTable(
    reinterpret_cast<uint64_t*>(
      arch::aarch64::DescriptorAddress(parent_descriptor)),
    child_level1, 1U);
  if (IsErr(status)) {
    (void)AddressSpaceDestroy(child);
    return status;
  }
  arch::aarch64::InvalidateTlbAllLocal();
  return Status::kOk;
}

}  // namespace og::mem

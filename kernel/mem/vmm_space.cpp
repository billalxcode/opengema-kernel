#include "og/mem/vmm.h"

#include "memory.h"
#include "og/arch/cache.h"
#include "og/arch/descriptor.h"
#include "og/arch/tlb.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm_internal.h"

namespace og::mem {

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
  space.mmap_next = kUserSpaceBase + 0x1000000ULL;
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

}  // namespace og::mem

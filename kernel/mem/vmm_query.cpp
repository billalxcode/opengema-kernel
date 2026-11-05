#include "og/mem/vmm.h"

#include "og/arch/descriptor.h"
#include "og/arch/mmu.h"
#include "og/mem/vmm_internal.h"

namespace og::mem::detail {

uint64_t Index(uint64_t virtual_address, unsigned level) noexcept {
  const unsigned shift = 39U - (level * 9U);
  return (virtual_address >> shift) & (kTableEntries - 1U);
}

Result<uint64_t> TranslateInRoot(uint64_t* root,
                                 uint64_t virtual_address) noexcept {
  if (root == nullptr) {
    return Result<uint64_t>::Err(Status::kNotInit);
  }

  uint64_t* table = root;
  for (unsigned level = 0; level < 3; ++level) {
    const uint64_t descriptor = table[Index(virtual_address, level)];
    if (!arch::aarch64::DescriptorValid(descriptor)) {
      return Result<uint64_t>::Err(Status::kNotFound);
    }
    if (level >= 1U && arch::aarch64::DescriptorIsBlock(descriptor)) {
      const uint64_t block =
        (level == 1U) ? arch::aarch64::kBlockSize1G
                      : arch::aarch64::kBlockSize2M;
      return Result<uint64_t>::Ok(
        arch::aarch64::DescriptorAddress(descriptor) |
        (virtual_address & (block - 1U)));
    }
    if (!arch::aarch64::DescriptorIsTable(descriptor)) {
      return Result<uint64_t>::Err(Status::kNotFound);
    }
    table = reinterpret_cast<uint64_t*>(
      arch::aarch64::DescriptorAddress(descriptor));
  }

  const uint64_t descriptor = table[Index(virtual_address, 3)];
  if (!arch::aarch64::DescriptorValid(descriptor)) {
    return Result<uint64_t>::Err(Status::kNotFound);
  }
  return Result<uint64_t>::Ok(
    arch::aarch64::DescriptorAddress(descriptor) |
    (virtual_address & (arch::aarch64::kPageSize - 1U)));
}

}  // namespace og::mem::detail

namespace og::mem {

namespace {

[[nodiscard]] PageAttributes DecodeAttributes(uint64_t descriptor) noexcept {
  PageAttributes attributes{};
  attributes.executable_el1 = (descriptor & arch::aarch64::kPxn) == 0;
  attributes.executable_el0 = (descriptor & arch::aarch64::kUxn) == 0;
  const uint64_t access = descriptor & (3ULL << arch::aarch64::kApShift);
  // AP[1] clears write access for both EL1 and EL0; AP[0] grants EL0 access.
  attributes.writable =
    (access & (1ULL << (arch::aarch64::kApShift + 1U))) == 0U;
  attributes.user = (access & (1ULL << arch::aarch64::kApShift)) != 0U;
  return attributes;
}

}  // namespace

Result<PageAttributes> VmmQueryAttributes(uint64_t virtual_address) noexcept {
  auto* root = reinterpret_cast<uint64_t*>(VmmRootAddress());
  if (root == nullptr) {
    return Result<PageAttributes>::Err(Status::kNotInit);
  }

  uint64_t* table = root;
  for (unsigned level = 0; level < 3; ++level) {
    const uint64_t descriptor =
      table[detail::Index(virtual_address, static_cast<unsigned>(level))];
    if (!arch::aarch64::DescriptorValid(descriptor)) {
      return Result<PageAttributes>::Err(Status::kNotFound);
    }
    if (level >= 1U && arch::aarch64::DescriptorIsBlock(descriptor)) {
      return Result<PageAttributes>::Ok(DecodeAttributes(descriptor));
    }
    if (!arch::aarch64::DescriptorIsTable(descriptor)) {
      return Result<PageAttributes>::Err(Status::kNotFound);
    }
    table = reinterpret_cast<uint64_t*>(
      arch::aarch64::DescriptorAddress(descriptor));
  }

  const uint64_t descriptor = table[detail::Index(virtual_address, 3)];
  if (!arch::aarch64::DescriptorValid(descriptor)) {
    return Result<PageAttributes>::Err(Status::kNotFound);
  }
  return Result<PageAttributes>::Ok(DecodeAttributes(descriptor));
}

Result<uint64_t> VmmTranslate(uint64_t virtual_address) noexcept {
  return detail::TranslateInRoot(
    reinterpret_cast<uint64_t*>(VmmRootAddress()), virtual_address);
}

Result<uint64_t> VmmTranslateIn(const AddressSpace& space,
                                uint64_t virtual_address) noexcept {
  return detail::TranslateInRoot(reinterpret_cast<uint64_t*>(space.root),
                                 virtual_address);
}

}  // namespace og::mem

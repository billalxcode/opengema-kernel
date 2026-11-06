#include "og/mem/vmm.h"

#include "memory.h"

namespace og::mem {

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

namespace {

[[nodiscard]] Status CopyUser(const AddressSpace& space, uint64_t user_address,
                              void* kernel_address, uint64_t size,
                              bool to_user) noexcept {
  uint64_t offset = 0;
  while (offset < size) {
    const uint64_t current = user_address + offset;
    Result<uint64_t> physical = VmmTranslateIn(space, current);
    if (physical.is_err()) {
      // The copy may be the first access to a lazy mmap page.
      const Status faulted =
        VmmFaultUserPage(const_cast<AddressSpace&>(space), current);
      if (IsErr(faulted)) {
        return physical.status();
      }
      physical = VmmTranslateIn(space, current);
      if (physical.is_err()) {
        return physical.status();
      }
    }
    const uint64_t page_remaining =
      arch::aarch64::kPageSize - (current % arch::aarch64::kPageSize);
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

}  // namespace

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

}  // namespace og::mem

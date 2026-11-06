#include "gemapi/memory.h"

#include "error_internal.h"
#include "kgema/memory.h"

namespace gemapi::memory {

Result<void*> map(usize size) noexcept {
  if (size == 0U) {
    return Status::invalid_argument;
  }
  void* address = kgema::mmap(size);
  if (address == nullptr) {
    return detail::LastError(Status::no_memory);
  }
  return address;
}

Status unmap(void* address, usize size) noexcept {
  if (address == nullptr || size == 0U) {
    return Status::invalid_argument;
  }
  return kgema::munmap(address, size) ? Status::ok
                                      : detail::LastError(Status::io);
}

}  // namespace gemapi::memory

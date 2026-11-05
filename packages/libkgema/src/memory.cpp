#include "kgema/memory.h"

#include "kgema/syscall.h"

namespace kgema {

void* mmap(usize size) noexcept {
  if (size == 0) {
    return nullptr;
  }
  const i64 result = syscall::invoke3(syscall::kMmap, static_cast<i64>(size), 0, 0);
  if (result <= 0) {
    return nullptr;
  }
  return reinterpret_cast<void*>(result);
}

void* mmap_executable(usize size) noexcept {
  if (size == 0) {
    return nullptr;
  }
  const i64 result =
    syscall::invoke3(syscall::kMmap, static_cast<i64>(size), 1, 0);
  if (result <= 0) {
    return nullptr;
  }
  return reinterpret_cast<void*>(result);
}

bool munmap(void* address, usize size) noexcept {
  if (address == nullptr || size == 0) {
    return false;
  }
  const i64 result = syscall::invoke3(syscall::kMunmap,
                                      reinterpret_cast<i64>(address),
                                      static_cast<i64>(size), 0);
  return result == 0;
}

bool mprotect(void* address, usize size, bool executable,
              bool writable) noexcept {
  if (address == nullptr || size == 0 || (executable && writable)) {
    return false;
  }
  i64 flags = 0;
  if (executable) {
    flags |= 1;
  }
  if (writable) {
    flags |= 2;
  }
  const i64 result = syscall::invoke3(syscall::kMprotect,
                                      reinterpret_cast<i64>(address),
                                      static_cast<i64>(size), flags);
  return result == 0;
}

}  // namespace kgema

#include "kgema/file.h"

#include "kgema/syscall.h"

namespace kgema {

i64 open(const char* path, u64 flags) noexcept {
  if (path == nullptr) {
    return -1;
  }
  return syscall::invoke3(syscall::kOpen, reinterpret_cast<i64>(path),
                          static_cast<i64>(flags), 0);
}

bool close(i64 descriptor) noexcept {
  return syscall::invoke3(syscall::kClose, descriptor, 0, 0) == 0;
}

i64 read(i64 descriptor, void* buffer, usize size) noexcept {
  if (buffer == nullptr || size == 0U) {
    return -1;
  }
  return syscall::invoke3(syscall::kRead, descriptor,
                          reinterpret_cast<i64>(buffer),
                          static_cast<i64>(size));
}

bool seek(i64 descriptor, u64 offset) noexcept {
  return syscall::invoke3(syscall::kSeek, descriptor,
                          static_cast<i64>(offset), 0) == 0;
}

bool stat(const char* path, Stat& result) noexcept {
  if (path == nullptr) {
    return false;
  }
  return syscall::invoke3(syscall::kStat, reinterpret_cast<i64>(path),
                          reinterpret_cast<i64>(&result), 0) == 0;
}

}  // namespace kgema

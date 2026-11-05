#include "kgema/process.h"

#include "kgema/syscall.h"

namespace kgema {

[[noreturn]] void exit(int status) noexcept {
  (void)syscall::invoke3(syscall::kExit, static_cast<i64>(status), 0, 0);
  for (;;) {
  }
}

u64 getpid() noexcept {
  return static_cast<u64>(syscall::invoke3(syscall::kGetPid, 0, 0, 0));
}

i64 spawn(const char* path, const char* const* argv, usize argc) noexcept {
  if (path == nullptr) {
    return -1;
  }
  return syscall::invoke3(syscall::kSpawn, reinterpret_cast<i64>(path),
                          reinterpret_cast<i64>(argv), static_cast<i64>(argc));
}

i64 wait(i64 pid, int* status) noexcept {
  return syscall::invoke3(syscall::kWait, pid,
                          reinterpret_cast<i64>(status), 0);
}

bool uid(u64& value) noexcept {
  const i64 result = syscall::invoke3(syscall::kGetUid, 0, 0, 0);
  if (result < 0) {
    return false;
  }
  value = static_cast<u64>(result);
  return true;
}

bool setuid(u64 value) noexcept {
  return syscall::invoke3(syscall::kSetUid, static_cast<i64>(value), 0, 0) ==
         0;
}

bool gid(u64& value) noexcept {
  const i64 result = syscall::invoke3(syscall::kGetGid, 0, 0, 0);
  if (result < 0) {
    return false;
  }
  value = static_cast<u64>(result);
  return true;
}

bool setgid(u64 value) noexcept {
  return syscall::invoke3(syscall::kSetGid, static_cast<i64>(value), 0, 0) ==
         0;
}

}  // namespace kgema

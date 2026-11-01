#include "gemapi/process.h"

#include "kgema/process.h"

namespace gemapi::process {

[[noreturn]] void exit(int status) noexcept { kgema::exit(status); }

u64 id() noexcept { return kgema::getpid(); }

Result<u64> spawn(const char* path, const char* const* argv, usize argc) noexcept {
  if (path == nullptr) {
    return Status::invalid_argument;
  }
  const i64 pid = kgema::spawn(path, argv, argc);
  if (pid <= 0) {
    return Status::not_found;
  }
  return static_cast<u64>(pid);
}

Status wait(u64 pid, int& status) noexcept {
  const i64 result = kgema::wait(static_cast<i64>(pid), &status);
  if (result < 0) {
    return Status::io;
  }
  return Status::ok;
}

}  // namespace gemapi::process

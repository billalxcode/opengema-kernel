#include "og/syscall/syscall_internal.h"

#include "memory.h"
#include "og/fs/vfs.h"

namespace og::syscall::detail {

process::Process* CurrentProcess() noexcept {
  return process::Current();
}

Status CopyUserString(const mem::AddressSpace& space, uint64_t user_address,
                      char* destination, uint64_t capacity) noexcept {
  if (destination == nullptr || capacity < 2U) {
    return Status::kInvalidArg;
  }
  for (uint64_t index = 0; index < (capacity - 1U); ++index) {
    char value = '\0';
    const Status status = mem::VmmCopyIn(space, user_address + index, &value, 1);
    if (IsErr(status)) {
      return status;
    }
    destination[index] = value;
    if (value == '\0') {
      return Status::kOk;
    }
  }
  destination[capacity - 1U] = '\0';
  return Status::kInvalidArg;
}

Status CopyUserPath(process::Process& process, uint64_t user_path,
                    char* destination) noexcept {
  char relative[fs::kMaxPath];
  const Status status =
    CopyUserString(process.space, user_path, relative, sizeof(relative));
  if (IsErr(status)) {
    return status;
  }
  if (relative[0] == '/') {
    const std::size_t length = mem::Length(relative);
    if (length >= fs::kMaxPath) {
      return Status::kInvalidArg;
    }
    mem::Copy(destination, relative, length + 1U);
    return Status::kOk;
  }

  const std::size_t cwd_length = mem::Length(process.cwd);
  const std::size_t relative_length = mem::Length(relative);
  const bool needs_separator = cwd_length > 1U;
  const std::size_t total =
    cwd_length + (needs_separator ? 1U : 0U) + relative_length;
  if (total >= fs::kMaxPath) {
    return Status::kInvalidArg;
  }
  mem::Copy(destination, process.cwd, cwd_length);
  std::size_t offset = cwd_length;
  if (needs_separator) {
    destination[offset] = '/';
    ++offset;
  }
  mem::Copy(destination + offset, relative, relative_length + 1U);
  return Status::kOk;
}

}  // namespace og::syscall::detail

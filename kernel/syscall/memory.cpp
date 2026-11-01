#include "og/syscall/syscall_internal.h"

#include "og/mem/vmm.h"

namespace og::syscall {

using namespace detail;

Status DoMmap(uint64_t size, uint64_t flags, uint64_t& result) noexcept {
  if (size == 0U || flags > kMmapFlagExecutable) {
    return Status::kInvalidArg;
  }
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (size > mem::kUserSpaceSize) {
    return Status::kInvalidArg;
  }
  const uint64_t aligned = (size + kPageSize - 1U) & ~(kPageSize - 1U);
  if (process->space.mmap_next >= kUserMmapLimit ||
      aligned > (kUserMmapLimit - process->space.mmap_next)) {
    return Status::kNoMem;
  }
  const uint64_t address = process->space.mmap_next;
  const bool executable = (flags & kMmapFlagExecutable) != 0U;
  const Status status =
    mem::VmmMapUser(process->space, address, aligned, executable);
  if (IsErr(status)) {
    return status;
  }
  process->space.mmap_next += aligned;
  result = address;
  return Status::kOk;
}

Status DoMunmap(uint64_t address, uint64_t size, uint64_t& result) noexcept {
  if (size == 0U) {
    return Status::kInvalidArg;
  }
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (size > mem::kUserSpaceSize) {
    return Status::kInvalidArg;
  }
  const uint64_t aligned = (size + kPageSize - 1U) & ~(kPageSize - 1U);
  const Status status = mem::VmmUnmapUser(process->space, address, aligned);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

}  // namespace og::syscall

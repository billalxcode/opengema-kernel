#include "og/syscall/syscall_internal.h"

#include "og/mem/pmm.h"
#include "og/mem/vmm.h"

namespace og::syscall {

using namespace detail;

Status DoMmap(uint64_t size, uint64_t flags, uint64_t& result) noexcept {
  if (size == 0U || (flags & ~kMmapFlagMask) != 0U) {
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
  const bool lazy = (flags & kMmapFlagLazy) != 0U;
  const Status status =
    lazy ? mem::VmmMapUserLazy(process->space, address, aligned, executable,
                               !executable)
         : mem::VmmMapUser(process->space, address, aligned, executable,
                           !executable);
  if (IsErr(status)) {
    if (status == Status::kNoMem &&
        mem::PmmFreeFrames() <= mem::kOomReserveFrames) {
      process::OutOfMemory(*process);
    }
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

Status DoMprotect(uint64_t address, uint64_t size, uint64_t flags,
                  uint64_t& result) noexcept {
  constexpr uint64_t kAllowed =
    kProtectFlagExecutable | kProtectFlagWritable;
  const bool executable = (flags & kProtectFlagExecutable) != 0U;
  const bool writable = (flags & kProtectFlagWritable) != 0U;
  if (size == 0U || (flags & ~kAllowed) != 0U || (executable && writable)) {
    return Status::kInvalidArg;
  }
  if ((address % kPageSize) != 0U || address < mem::kUserSpaceBase) {
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
  if (address > (kUserMmapLimit - aligned)) {
    return Status::kInvalidArg;
  }
  if (!mem::VmmUserContains(process->space, address, aligned)) {
    return Status::kInvalidArg;
  }
  const Status status =
    mem::VmmProtectUser(process->space, address, aligned, executable, writable);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

}  // namespace og::syscall

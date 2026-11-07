#include "og/syscall/syscall_internal.h"

#include "og/fs/vfs.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm.h"

namespace og::syscall {

using namespace detail;

namespace {

// Copies file contents into an already mapped region. Pages beyond EOF stay
// zero because VmmMapUser zero-fills every frame. The destination frame is
// reached through the kernel identity map, so no bounce buffer is needed.
[[nodiscard]] Status FillFileMapping(process::Process& process,
                                     uint64_t address, uint64_t size,
                                     uint32_t descriptor,
                                     uint64_t offset) noexcept {
  uint64_t copied = 0;
  while (copied < size) {
    const uint64_t remaining = size - copied;
    const uint64_t chunk =
      remaining > kPageSize ? kPageSize : remaining;
    const Result<uint64_t> physical =
      mem::VmmTranslateIn(process.space, address + copied);
    if (physical.is_err()) {
      return physical.status();
    }
    auto* destination = reinterpret_cast<uint8_t*>(physical.value());
    uint64_t read = 0;
    const Status read_status = fs::FsReadAt(
      process.files, descriptor, offset + copied,
      std::span<uint8_t>(destination, chunk), read);
    if (IsErr(read_status)) {
      return read_status;
    }
    if (read == 0U) {
      break;
    }
    copied += read;
  }
  return Status::kOk;
}

}  // namespace

Status DoMmap(uint64_t size, uint64_t flags, uint64_t descriptor,
              uint64_t offset, uint64_t& result) noexcept {
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
  const bool executable = (flags & kMmapFlagExecutable) != 0U;
  const bool lazy = (flags & kMmapFlagLazy) != 0U;
  const bool shared = (flags & kMmapFlagShared) != 0U;
  const bool file_backed = (flags & kMmapFlagFile) != 0U;
  if ((shared && lazy) || (file_backed && (lazy || shared))) {
    return Status::kInvalidArg;
  }

  uint32_t file_descriptor = 0;
  if (file_backed) {
    if ((offset % kPageSize) != 0U || descriptor > 0xFFFFFFFFULL) {
      return Status::kInvalidArg;
    }
    file_descriptor = static_cast<uint32_t>(descriptor);
    uint64_t file_size = 0;
    const Status file_status =
      fs::FsMmapSize(process->files, file_descriptor, file_size);
    if (IsErr(file_status)) {
      return file_status;
    }
    if (offset > file_size || (offset + aligned) < offset) {
      return Status::kInvalidArg;
    }
  }

  if (process->space.mmap_next >= kUserMmapLimit ||
      aligned > (kUserMmapLimit - process->space.mmap_next)) {
    return Status::kNoMem;
  }
  const uint64_t address = process->space.mmap_next;
  const Status status =
    lazy ? mem::VmmMapUserLazy(process->space, address, aligned, executable,
                               !executable)
         : mem::VmmMapUser(process->space, address, aligned, executable,
                           !executable, shared);
  if (IsErr(status)) {
    if (status == Status::kNoMem &&
        mem::PmmFreeFrames() <= mem::kOomReserveFrames) {
      process::OutOfMemory(*process);
    }
    return status;
  }

  if (file_backed) {
    const Status fill = FillFileMapping(*process, address, aligned,
                                        file_descriptor, offset);
    if (IsErr(fill)) {
      (void)mem::VmmUnmapUser(process->space, address, aligned);
      return fill;
    }
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

#include "og/syscall/syscall.h"

#include "memory.h"
#include "og/arch/registers.h"
#include "og/arch/trap.h"
#include "og/core/log.h"
#include "og/core/status.h"
#include "og/events/irq.h"
#include "og/fs/vfs.h"
#include "og/io/print.h"
#include "og/mem/vmm.h"
#include "og/process/process.h"
#include "og/sched/sched.h"

namespace og::syscall {

namespace {

constexpr uint64_t kEsrEcShift = 26;
constexpr uint64_t kEsrEcMask = 0x3F;
constexpr uint64_t kExceptionClassSvc64 = 0x15;
constexpr uint64_t kSyscallNumberRegister = 8;
constexpr uint64_t kMaxWriteChunk = 256;
constexpr uint64_t kMaxReadChunk = 256;
constexpr uint64_t kStdout = 1;
constexpr uint64_t kResultError = static_cast<uint64_t>(-1);
constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kUserMmapLimit = mem::kUserSpaceBase + mem::kUserSpaceSize;

struct UserStat {
  uint64_t type;
  uint64_t size;
};

[[nodiscard]] process::Process* CurrentProcess() noexcept {
  return process::Current();
}

[[nodiscard]] Status CopyUserString(const mem::AddressSpace& space,
                                    uint64_t user_address, char* destination,
                                    uint64_t capacity) noexcept {
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

[[nodiscard]] Status CopyUserPath(process::Process& process, uint64_t user_path,
                                  char* destination) noexcept {
  char relative[fs::kMaxPath];
  Status status = CopyUserString(process.space, user_path, relative, sizeof(relative));
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

[[nodiscard]] Status DoMmap(uint64_t size, uint64_t& result) noexcept {
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
  if (process->space.mmap_next >= kUserMmapLimit ||
      aligned > (kUserMmapLimit - process->space.mmap_next)) {
    return Status::kNoMem;
  }
  const uint64_t address = process->space.mmap_next;
  const Status status = mem::VmmMapUser(process->space, address, aligned, false);
  if (IsErr(status)) {
    return status;
  }
  process->space.mmap_next += aligned;
  result = address;
  return Status::kOk;
}

[[nodiscard]] Status DoMunmap(uint64_t address, uint64_t size,
                              uint64_t& result) noexcept {
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

[[nodiscard]] Status DoOpen(uint64_t user_path, uint64_t flags,
                            uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  const Result<uint32_t> descriptor =
    fs::FsOpen(process->files, path, static_cast<uint32_t>(flags));
  if (descriptor.is_err()) {
    return descriptor.status();
  }
  result = descriptor.value();
  return Status::kOk;
}

[[nodiscard]] Status DoClose(uint64_t descriptor, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  const Status status = fs::FsClose(process->files, static_cast<uint32_t>(descriptor));
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

[[nodiscard]] Status DoRead(uint64_t descriptor, uint64_t user_buffer,
                            uint64_t length, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (length == 0U) {
    result = 0;
    return Status::kOk;
  }

  const uint64_t chunk = length > kMaxReadChunk ? kMaxReadChunk : length;
  uint8_t buffer[kMaxReadChunk];
  uint64_t read = 0;
  Status status = fs::FsRead(process->files, static_cast<uint32_t>(descriptor),
                             std::span<uint8_t>(buffer, chunk), read);
  if (IsErr(status)) {
    return status;
  }
  if (read > 0U) {
    status = mem::VmmCopyOut(process->space, user_buffer, buffer, read);
    if (IsErr(status)) {
      return status;
    }
  }
  result = read;
  return Status::kOk;
}

[[nodiscard]] Status DoSeek(uint64_t descriptor, uint64_t offset,
                            uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  const Status status =
    fs::FsSeek(process->files, static_cast<uint32_t>(descriptor), offset);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

[[nodiscard]] Status DoStat(uint64_t user_path, uint64_t user_stat,
                            uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  const Result<fs::Stat> stat = fs::FsStat(path);
  if (stat.is_err()) {
    return stat.status();
  }
  UserStat output{static_cast<uint64_t>(stat.value().type), stat.value().size};
  status = mem::VmmCopyOut(process->space, user_stat, &output, sizeof(output));
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

[[nodiscard]] Status DoWrite(uint64_t descriptor, uint64_t user_buffer,
                             uint64_t length, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (length == 0U) {
    result = 0;
    return Status::kOk;
  }
  if (descriptor > 0xFFFFFFFFULL) {
    return Status::kInvalidArg;
  }

  char text[kMaxWriteChunk + 1U];
  uint64_t total = 0;
  while (total < length) {
    const uint64_t remaining = length - total;
    const uint64_t chunk = remaining > kMaxWriteChunk ? kMaxWriteChunk : remaining;
    const Status copy =
      mem::VmmCopyIn(process->space, user_buffer + total, text, chunk);
    if (IsErr(copy)) {
      LogHex(LogLevel::kError, "syscall: copy failed", static_cast<uint64_t>(copy));
      return copy;
    }

    if (descriptor == kStdout) {
      text[chunk] = '\0';
      const Status status = PrintText(text);
      if (IsErr(status)) {
        LogHex(LogLevel::kError, "syscall: write failed", static_cast<uint64_t>(status));
        return status;
      }
      total += chunk;
      continue;
    }

    uint64_t written = 0;
    const Status status = fs::FsWrite(
      process->files, static_cast<uint32_t>(descriptor),
      std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text), chunk), written);
    if (IsErr(status)) {
      return status;
    }
    if (written == 0U) {
      return Status::kIoError;
    }
    total += written;
  }

  result = total;
  return Status::kOk;
}

[[nodiscard]] Status DoGetPid(uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  result = process->pid;
  return Status::kOk;
}

[[nodiscard]] Status DoChdir(uint64_t user_path, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  const Result<fs::Stat> stat = fs::FsStat(path);
  if (stat.is_err()) {
    return stat.status();
  }
  if (stat.value().type != fs::NodeType::kDirectory) {
    return Status::kInvalidArg;
  }
  status = process::SetCwd(*process, path);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

[[nodiscard]] Status DoWait(uint64_t pid, uint64_t user_status,
                            uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (user_status != 0U &&
      !mem::VmmUserContains(process->space, user_status, sizeof(int32_t))) {
    return Status::kInvalidArg;
  }
  int32_t status = 0;
  const Result<uint32_t> child =
    process::WaitChild(process->pid, static_cast<uint32_t>(pid), status);
  if (child.is_err()) {
    return child.status();
  }
  if (user_status != 0U) {
    const Status copied =
      mem::VmmCopyOut(process->space, user_status, &status, sizeof(status));
    if (IsErr(copied)) {
      return copied;
    }
  }
  result = child.value();
  return Status::kOk;
}

[[nodiscard]] Status DoSpawn(uint64_t user_path, uint64_t user_argv,
                            uint64_t argc, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  if (argc == 0U) {
    const Result<uint32_t> child = process::Spawn(path, process->pid);
    if (child.is_err()) {
      return child.status();
    }
    result = child.value();
    return Status::kOk;
  }
  if (argc > process::kMaxArgs || user_argv == 0U) {
    return Status::kInvalidArg;
  }

  char storage[process::kMaxArgs][process::kMaxArgLength];
  const char* values[process::kMaxArgs];
  for (uint32_t index = 0; index < argc; ++index) {
    uint64_t user_string = 0;
    status = mem::VmmCopyIn(process->space, user_argv + (index * sizeof(uint64_t)),
                            &user_string, sizeof(user_string));
    if (IsErr(status)) {
      return status;
    }
    status = CopyUserString(process->space, user_string, storage[index],
                            process::kMaxArgLength);
    if (IsErr(status)) {
      return status;
    }
    values[index] = storage[index];
  }

  const Result<uint32_t> child =
    process::SpawnWithArgs(path, process->pid, values, static_cast<uint32_t>(argc));
  if (child.is_err()) {
    return child.status();
  }
  result = child.value();
  return Status::kOk;
}

[[nodiscard]] Status Dispatch(arch::aarch64::TrapFrame* frame) noexcept {
  if (frame == nullptr) {
    return Status::kInvalidArg;
  }

  const uint64_t esr = arch::aarch64::ReadEsrEl1();
  const uint64_t exception_class = (esr >> kEsrEcShift) & kEsrEcMask;
  if (exception_class != kExceptionClassSvc64) {
    return Status::kUnsupported;
  }

  const uint64_t number = frame->x[kSyscallNumberRegister];
  uint64_t result = 0;
  Status status = Status::kOk;

  switch (number) {
    case kSysWrite:
      status = DoWrite(frame->x[0], frame->x[1], frame->x[2], result);
      break;
    case kSysRead:
      status = DoRead(frame->x[0], frame->x[1], frame->x[2], result);
      break;
    case kSysExit:
      process::Exit(static_cast<int32_t>(frame->x[0]));
    case kSysYield:
      sched::ThreadYield();
      result = 0;
      break;
    case kSysMmap:
      status = DoMmap(frame->x[0], result);
      break;
    case kSysMunmap:
      status = DoMunmap(frame->x[0], frame->x[1], result);
      break;
    case kSysOpen:
      status = DoOpen(frame->x[0], frame->x[1], result);
      break;
    case kSysClose:
      status = DoClose(frame->x[0], result);
      break;
    case kSysSeek:
      status = DoSeek(frame->x[0], frame->x[1], result);
      break;
    case kSysStat:
      status = DoStat(frame->x[0], frame->x[1], result);
      break;
    case kSysGetPid:
      status = DoGetPid(result);
      break;
    case kSysWait:
      status = DoWait(frame->x[0], frame->x[1], result);
      break;
    case kSysSpawn:
      status = DoSpawn(frame->x[0], frame->x[1], frame->x[2], result);
      break;
    case kSysChdir:
      status = DoChdir(frame->x[0], result);
      break;
    default:
      status = Status::kInvalidArg;
      break;
  }

  if (IsErr(status)) {
    result = kResultError;
  }
  frame->x[0] = result;
  return Status::kOk;
}

[[nodiscard]] Status SyncHandler(void* /*context*/,
                                 arch::aarch64::TrapFrame* frame) noexcept {
  return Dispatch(frame);
}

}  // namespace

void SyscallInit() noexcept {
  events::SetSyncHook(&SyncHandler, nullptr);
}

}  // namespace og::syscall

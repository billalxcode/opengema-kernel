#include "og/syscall.h"

#include "og/arch/registers.h"
#include "og/arch/trap.h"
#include "og/console.h"
#include "og/irq.h"
#include "og/log.h"
#include "og/sched.h"
#include "og/status.h"
#include "og/vmm.h"

namespace og::syscall {

namespace {

constexpr uint64_t kEsrEcShift = 26;
constexpr uint64_t kEsrEcMask = 0x3F;
constexpr uint64_t kExceptionClassSvc64 = 0x15;
constexpr uint64_t kSyscallNumberRegister = 8;
constexpr uint64_t kMaxWriteChunk = 256;
constexpr uint64_t kStdout = 1;
constexpr uint64_t kResultError = static_cast<uint64_t>(-1);
constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kUserMmapBase = 0x101000000ULL;
constexpr uint64_t kUserMmapLimit = 0x102000000ULL;

bool g_user_exited = false;
uint64_t g_mmap_next = kUserMmapBase;

[[nodiscard]] Status DoMmap(uint64_t size, uint64_t& result) noexcept {
  if (size == 0) {
    return Status::kInvalidArg;
  }
  const uint64_t aligned = (size + kPageSize - 1U) & ~(kPageSize - 1U);
  if (aligned > (kUserMmapLimit - g_mmap_next)) {
    return Status::kNoMem;
  }
  const uint64_t address = g_mmap_next;
  const Status status = mem::VmmMapUser(address, aligned, false);
  if (IsErr(status)) {
    return status;
  }
  g_mmap_next += aligned;
  result = address;
  return Status::kOk;
}

[[nodiscard]] Status DoMunmap(uint64_t address, uint64_t size, uint64_t& result) noexcept {
  if (size == 0) {
    return Status::kInvalidArg;
  }
  const uint64_t aligned = (size + kPageSize - 1U) & ~(kPageSize - 1U);
  const Status status = mem::VmmUnmapUser(address, aligned);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

[[nodiscard]] Status DoWrite(uint64_t descriptor, uint64_t user_buffer, uint64_t length,
                             uint64_t& result) noexcept {
  if (descriptor != kStdout) {
    return Status::kUnsupported;
  }
  if (length == 0) {
    result = 0;
    return Status::kOk;
  }
  const uint64_t chunk = length > kMaxWriteChunk ? kMaxWriteChunk : length;

  char text[kMaxWriteChunk + 1U];
  const Status copy = mem::VmmCopyIn(user_buffer, text, chunk);
  if (IsErr(copy)) {
    LogHex(LogLevel::kError, "syscall: copy failed", static_cast<uint64_t>(copy));
    return copy;
  }
  text[chunk] = '\0';

  const Status status = io::ConsoleWriteTextActive(text);
  if (IsErr(status)) {
    LogHex(LogLevel::kError, "syscall: write failed", static_cast<uint64_t>(status));
    return status;
  }
  result = chunk;
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
      status = Status::kUnsupported;
      break;
    case kSysExit:
      g_user_exited = true;
      sched::ThreadExit();
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

bool UserExited() noexcept {
  return g_user_exited;
}

}  // namespace og::syscall

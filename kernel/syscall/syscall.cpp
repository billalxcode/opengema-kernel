#include "og/syscall/syscall.h"

#include "og/arch/registers.h"
#include "og/arch/trap.h"
#include "og/core/status.h"
#include "og/events/irq.h"
#include "og/process/process.h"
#include "og/sched/sched.h"
#include "og/syscall/syscall_internal.h"

namespace og::syscall {

namespace {

constexpr uint64_t kEsrEcShift = 26;
constexpr uint64_t kEsrEcMask = 0x3F;
constexpr uint64_t kExceptionClassSvc64 = 0x15;
constexpr uint64_t kSyscallNumberRegister = 8;
constexpr uint64_t kResultError = static_cast<uint64_t>(-1);

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
      status = DoMmap(frame->x[0], frame->x[1], result);
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
    case kSysList:
      status = DoList(frame->x[0], frame->x[1], frame->x[2], result);
      break;
    case kSysShutdown:
      status = DoShutdown(result);
      break;
    case kSysReboot:
      status = DoReboot(result);
      break;
    case kSysTime:
      status = DoTime(result);
      break;
    case kSysSleep:
      status = DoSleep(frame->x[0], result);
      break;
    case kSysMkdir:
      status = DoMkdir(frame->x[0], result);
      break;
    case kSysUnlink:
      status = DoUnlink(frame->x[0], result);
      break;
    case kSysRmdir:
      status = DoRmdir(frame->x[0], result);
      break;
    case kSysRename:
      status = DoRename(frame->x[0], frame->x[1], result);
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

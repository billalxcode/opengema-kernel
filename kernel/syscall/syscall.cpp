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

[[nodiscard]] int64_t ErrnoFor(Status status) noexcept {
  switch (status) {
    case Status::kOk:
      return 0;
    case Status::kInvalidArg:
    case Status::kUnaligned:
      return kErrInvalid;
    case Status::kNotInit:
    case Status::kNoDevice:
      return kErrNoDevice;
    case Status::kNoMem:
      return kErrNoMem;
    case Status::kNotImpl:
      return kErrNoSys;
    case Status::kNotFound:
      return kErrNotFound;
    case Status::kExists:
      return kErrExists;
    case Status::kTimeout:
      return kErrTimeout;
    case Status::kIoError:
      return kErrIo;
    case Status::kUnsupported:
      return kErrUnsupported;
    case Status::kBusy:
      return kErrBusy;
    case Status::kDenied:
      return kErrDenied;
  }
  return kErrInvalid;
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
      status = DoMmap(frame->x[0], frame->x[1], frame->x[2], frame->x[3],
                      result);
      break;
    case kSysMunmap:
      status = DoMunmap(frame->x[0], frame->x[1], result);
      break;
    case kSysMprotect:
      status = DoMprotect(frame->x[0], frame->x[1], frame->x[2], result);
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
    case kSysFork:
      status = DoFork(frame, result);
      break;
    case kSysExec:
      status = DoExec(frame->x[0], frame->x[1], frame->x[2], frame, result);
      break;
    case kSysGetPriority:
      status = DoGetPriority(result);
      break;
    case kSysSetPriority:
      status = DoSetPriority(frame->x[0], result);
      break;
    case kSysFutex:
      status = DoFutex(frame->x[0], frame->x[1], frame->x[2], result);
      break;
    case kSysGetTls:
      status = DoGetTls(result);
      break;
    case kSysSetTls:
      status = DoSetTls(frame->x[0], result);
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
    case kSysGetLogLevel:
      status = DoGetLogLevel(result);
      break;
    case kSysSetLogLevel:
      status = DoSetLogLevel(frame->x[0], result);
      break;
    case kSysMount:
      status = DoMount(frame->x[0], frame->x[1], result);
      break;
    case kSysUmount:
      status = DoUmount(frame->x[0], result);
      break;
    case kSysWallTime:
      status = DoWallTime(result);
      break;
    case kSysSetWallTime:
      status = DoSetWallTime(frame->x[0], result);
      break;
    case kSysLocalTime:
      status = DoLocalTime(result);
      break;
    case kSysSetTimeZone:
      status = DoSetTimeZone(frame->x[0], result);
      break;
    case kSysGetTimeZone:
      status = DoGetTimeZone(result);
      break;
    case kSysChmod:
      status = DoChmod(frame->x[0], frame->x[1], result);
      break;
    case kSysGetUid:
      status = DoGetUid(result);
      break;
    case kSysSetUid:
      status = DoSetUid(frame->x[0], result);
      break;
    case kSysGetGid:
      status = DoGetGid(result);
      break;
    case kSysSetGid:
      status = DoSetGid(frame->x[0], result);
      break;
    case kSysChown:
      status = DoChown(frame->x[0], frame->x[1], frame->x[2], result);
      break;
    default:
      status = Status::kInvalidArg;
      break;
  }

  if (IsErr(status)) {
    result = static_cast<uint64_t>(ErrnoFor(status));
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

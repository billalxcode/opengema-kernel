#include "og/syscall/syscall_internal.h"

#include "og/arch/registers.h"
#include "og/fs/vfs.h"
#include "og/mem/vmm.h"
#include "og/sched/sched.h"

namespace og::syscall {

using namespace detail;

Status DoGetPid(uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  result = process->pid;
  return Status::kOk;
}

Status DoGetUid(uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  result = process->uid;
  return Status::kOk;
}

Status DoSetUid(uint64_t uid, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (uid > 0xFFFFFFFFULL) {
    return Status::kInvalidArg;
  }
  const uint32_t value = static_cast<uint32_t>(uid);
  if (process->uid != 0U && value != process->uid) {
    return Status::kDenied;
  }
  const Status status = process::SetUid(*process, value);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoGetGid(uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  result = process->gid;
  return Status::kOk;
}

Status DoSetGid(uint64_t gid, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (gid > 0xFFFFFFFFULL) {
    return Status::kInvalidArg;
  }
  const uint32_t value = static_cast<uint32_t>(gid);
  if (process->uid != 0U && value != process->gid) {
    return Status::kDenied;
  }
  const Status status = process::SetGid(*process, value);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoGetPriority(uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (process->thread == nullptr) {
    return Status::kNotInit;
  }
  result = sched::ThreadPriority(process->thread);
  return Status::kOk;
}

Status DoSetPriority(uint64_t priority, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (priority > sched::kPriorityLowest || process->thread == nullptr) {
    return Status::kInvalidArg;
  }
  const uint32_t value = static_cast<uint32_t>(priority);
  const uint32_t current = sched::ThreadPriority(process->thread);
  if (process->uid != 0U && value < current) {
    return Status::kDenied;
  }
  const Status status = sched::ThreadSetPriority(process->thread, value);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoGetTls(uint64_t& result) noexcept {
  if (CurrentProcess() == nullptr) {
    return Status::kNotInit;
  }
  result = arch::aarch64::ReadTpidrEl0();
  return Status::kOk;
}

Status DoSetTls(uint64_t address, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (address != 0U && !mem::VmmUserContains(process->space, address, 1U)) {
    return Status::kInvalidArg;
  }
  process->tls = address;
  if (process->thread != nullptr) {
    process->thread->tls = address;
  }
  arch::aarch64::WriteTpidrEl0(address);
  result = 0;
  return Status::kOk;
}

Status DoWait(uint64_t pid, uint64_t user_status, uint64_t& result) noexcept {
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

Status DoSpawn(uint64_t user_path, uint64_t user_argv, uint64_t argc,
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
    status = mem::VmmCopyIn(process->space,
                            user_argv + (index * sizeof(uint64_t)),
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

  const Result<uint32_t> child = process::SpawnWithArgs(
    path, process->pid, values, static_cast<uint32_t>(argc));
  if (child.is_err()) {
    return child.status();
  }
  result = child.value();
  return Status::kOk;
}

Status DoFork(arch::aarch64::TrapFrame* frame, uint64_t& result) noexcept {
  const Result<uint32_t> child = process::Fork(frame);
  if (child.is_err()) {
    return child.status();
  }
  result = child.value();
  return Status::kOk;
}

Status DoExec(uint64_t user_path, uint64_t user_argv, uint64_t argc,
              arch::aarch64::TrapFrame* frame, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  const char* values[process::kMaxArgs];
  char storage[process::kMaxArgs][process::kMaxArgLength];
  uint32_t count = static_cast<uint32_t>(argc);
  if (count == 0U) {
    values[0] = path;
    count = 1;
  } else {
    if (argc > process::kMaxArgs || user_argv == 0U) {
      return Status::kInvalidArg;
    }
    for (uint32_t index = 0; index < count; ++index) {
      uint64_t user_string = 0;
      status = mem::VmmCopyIn(process->space,
                              user_argv + (index * sizeof(uint64_t)),
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
  }

  status = process::Exec(path, values, count, frame);
  if (IsErr(status)) {
    return status;
  }
  result = count;
  return Status::kOk;
}

}  // namespace og::syscall

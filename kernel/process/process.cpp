#include "og/process/process.h"

#include "memory.h"
#include "og/arch/spinlock.h"
#include "og/arch/user.h"
#include "og/core/log.h"
#include "og/mem/heap.h"
#include "og/sched/sched.h"

namespace og::process {

namespace {

Process g_processes[kMaxProcesses];
arch::aarch64::IrqSpinlock g_lock;
sched::WaitQueue g_waiters;
uint32_t g_next_pid = 1;
bool g_ready = false;

struct UserStack {
  uint64_t stack_pointer;
  uint64_t argv;
  uint64_t envp;
  uint32_t argc;
};

[[nodiscard]] Status BuildUserStack(const mem::AddressSpace& space, const char* path,
                                    const char* const* argv, uint32_t argc,
                                    UserStack& out) noexcept {
  if (argc > kMaxArgs) {
    return Status::kInvalidArg;
  }

  const char* values[kMaxArgs];
  uint32_t count = argc;
  if (count == 0U) {
    values[0] = path;
    count = 1;
  } else {
    if (argv == nullptr) {
      return Status::kInvalidArg;
    }
    for (uint32_t index = 0; index < count; ++index) {
      if (argv[index] == nullptr) {
        return Status::kInvalidArg;
      }
      values[index] = argv[index];
    }
  }

  uint64_t cursor = kUserStackVa + kUserStackSize;
  uint64_t pointers[kMaxArgs + 1U]{};
  for (uint32_t index = 0; index < count; ++index) {
    const uint64_t length = mem::Length(values[index]);
    if (length > kMaxArgLength || (length + 1U) >= (cursor - kUserStackVa)) {
      return Status::kInvalidArg;
    }
    cursor -= (length + 1U);
    const Status copied = mem::VmmCopyOut(space, cursor, values[index], length + 1U);
    if (IsErr(copied)) {
      return copied;
    }
    pointers[index] = cursor;
  }

  cursor &= ~7ULL;
  const uint64_t zero = 0;
  cursor -= sizeof(zero);
  Status status = mem::VmmCopyOut(space, cursor, &zero, sizeof(zero));
  if (IsErr(status)) {
    return status;
  }
  out.envp = cursor;

  cursor -= (static_cast<uint64_t>(count) + 1U) * sizeof(uint64_t);
  status = mem::VmmCopyOut(space, cursor, pointers,
                           (static_cast<uint64_t>(count) + 1U) * sizeof(uint64_t));
  if (IsErr(status)) {
    return status;
  }
  out.argv = cursor;
  out.stack_pointer = cursor & ~15ULL;
  out.argc = count;
  if (out.stack_pointer < (kUserStackVa + 512U)) {
    return Status::kNoMem;
  }
  return Status::kOk;
}

void ProcessThreadEntry(void* argument) noexcept {
  auto* process = static_cast<Process*>(argument);
  arch::aarch64::EnterUserMode(process->entry, process->stack_top, process->argc,
                               process->argv, process->envp);
}

[[nodiscard]] uint32_t NextPidLocked() noexcept {
  uint32_t pid = g_next_pid;
  ++g_next_pid;
  if (g_next_pid == 0U) {
    g_next_pid = 1;
  }
  if (pid == 0U) {
    pid = g_next_pid;
    ++g_next_pid;
  }
  return pid;
}

void CloseFiles(Process& process) noexcept {
  for (uint32_t descriptor = 0; descriptor < fs::kMaxFiles; ++descriptor) {
    if (process.files.entries[descriptor].in_use) {
      (void)fs::FsClose(process.files, descriptor);
    }
  }
}

[[nodiscard]] Result<uint32_t> WaitInternal(bool any_parent, uint32_t parent_pid,
                                            uint32_t pid, int32_t& status) noexcept {
  for (;;) {
    Process* target = nullptr;
    bool child_exists = false;

    g_lock.lock();
    for (uint32_t index = 0; index < kMaxProcesses; ++index) {
      Process& process = g_processes[index];
      if (process.used == 0U) {
        continue;
      }
      if (pid != 0U && process.pid != pid) {
        continue;
      }
      if (!any_parent && process.parent_pid != parent_pid) {
        continue;
      }
      child_exists = true;
      if (process.state == ProcessState::kExited) {
        process.state = ProcessState::kReaping;
        target = &process;
        break;
      }
    }
    g_lock.unlock();

    if (target != nullptr) {
      uint32_t waits = 0;
      while (target->thread != nullptr && waits < 5000U) {
        const bool finished = sched::ThreadFinished(target->thread);
        const bool current =
          sched::ThreadCurrentOnCpu(target->thread->home_cpu) == target->thread;
        if (finished && !current) {
          break;
        }
        (void)sched::ThreadSleep(1);
        ++waits;
      }
      if (target->thread != nullptr && waits >= 5000U) {
        g_lock.lock();
        target->state = ProcessState::kExited;
        g_lock.unlock();
        return Result<uint32_t>::Err(Status::kTimeout);
      }

      (void)mem::AddressSpaceDestroy(target->space);
      CloseFiles(*target);
      if (target->thread != nullptr) {
        (void)sched::ThreadReclaim(target->thread);
      }
      status = target->exit_code;
      const uint32_t result = target->pid;

      g_lock.lock();
      target->used = 0;
      target->thread = nullptr;
      g_lock.unlock();
      sched::WaitQueueWakeAll(g_waiters);
      return Result<uint32_t>::Ok(result);
    }

    if (!child_exists) {
      return Result<uint32_t>::Err(Status::kNotFound);
    }
    sched::WaitQueueWait(g_waiters);
  }
}

}  // namespace

Status Init() noexcept {
  mem::Zero(g_processes, sizeof(g_processes));
  sched::WaitQueueInit(g_waiters);
  g_next_pid = 1;
  g_ready = true;
  return Status::kOk;
}

Result<uint32_t> Spawn(const char* path, uint32_t parent_pid) noexcept {
  return SpawnWithArgs(path, parent_pid, nullptr, 0);
}

Result<uint32_t> SpawnWithArgs(const char* path, uint32_t parent_pid,
                               const char* const* argv, uint32_t argc) noexcept {
  if (!g_ready) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  if (path == nullptr || argc > kMaxArgs) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }

  const Result<fs::Stat> stat = fs::FsStat(path);
  if (stat.is_err()) {
    return Result<uint32_t>::Err(stat.status());
  }
  const uint64_t size = stat.value().size;
  if (size == 0U || size > kMaxProgramSize) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }

  const Result<void*> buffer = mem::HeapAlloc(size);
  if (buffer.is_err()) {
    return Result<uint32_t>::Err(buffer.status());
  }

  fs::FileTable loader_files{};
  (void)fs::FileTableInit(loader_files);
  const Result<uint32_t> file = fs::FsOpen(loader_files, path);
  if (file.is_err()) {
    (void)mem::HeapFree(buffer.value());
    return Result<uint32_t>::Err(file.status());
  }
  uint64_t read = 0;
  const Status read_status = fs::FsRead(
    loader_files, file.value(),
    std::span<uint8_t>(static_cast<uint8_t*>(buffer.value()), size), read);
  (void)fs::FsClose(loader_files, file.value());
  if (IsErr(read_status) || read != size) {
    (void)mem::HeapFree(buffer.value());
    return Result<uint32_t>::Err(Status::kIoError);
  }

  mem::AddressSpace space{};
  Status status = mem::AddressSpaceInit(space);
  if (IsErr(status)) {
    (void)mem::HeapFree(buffer.value());
    return Result<uint32_t>::Err(status);
  }
  const uint64_t code_size = (size + 4095U) & ~4095U;
  status = mem::VmmMapUser(space, kUserCodeVa, code_size, true);
  if (IsOk(status)) {
    status = mem::VmmCopyOut(space, kUserCodeVa, buffer.value(), size);
  }
  if (IsOk(status)) {
    status = mem::VmmMapUser(space, kUserStackVa, kUserStackSize, false);
  }
  UserStack user_stack{};
  if (IsOk(status)) {
    status = BuildUserStack(space, path, argv, argc, user_stack);
  }
  (void)mem::HeapFree(buffer.value());
  if (IsErr(status)) {
    (void)mem::AddressSpaceDestroy(space);
    return Result<uint32_t>::Err(status);
  }

  g_lock.lock();
  Process* slot = nullptr;
  for (uint32_t index = 0; index < kMaxProcesses; ++index) {
    if (g_processes[index].used == 0U) {
      slot = &g_processes[index];
      break;
    }
  }
  if (slot == nullptr) {
    g_lock.unlock();
    (void)mem::AddressSpaceDestroy(space);
    return Result<uint32_t>::Err(Status::kNoMem);
  }

  mem::Zero(slot, sizeof(*slot));
  slot->pid = NextPidLocked();
  slot->parent_pid = parent_pid;
  slot->state = ProcessState::kReady;
  slot->exit_code = 0;
  slot->used = 1;
  slot->space = space;
  slot->entry = kUserCodeVa;
  slot->stack_top = user_stack.stack_pointer;
  slot->argv = user_stack.argv;
  slot->envp = user_stack.envp;
  slot->argc = user_stack.argc;
  (void)fs::FileTableInit(slot->files);
  const std::size_t name_length = mem::Length(path);
  const std::size_t name_copy =
    name_length >= sizeof(slot->name) ? (sizeof(slot->name) - 1U) : name_length;
  mem::Copy(slot->name, path, name_copy);
  slot->name[name_copy] = '\0';
  slot->cwd[0] = '/';
  slot->cwd[1] = '\0';

  const Result<sched::Thread*> thread = sched::ThreadCreateIn(
    "user", &ProcessThreadEntry, slot, &slot->space, slot);
  if (thread.is_err()) {
    slot->used = 0;
    g_lock.unlock();
    (void)mem::AddressSpaceDestroy(space);
    return Result<uint32_t>::Err(thread.status());
  }
  slot->thread = thread.value();
  const uint32_t pid = slot->pid;
  g_lock.unlock();

  LogDec(LogLevel::kInfo, "user: program bytes", size);
  LogDec(LogLevel::kInfo, "user: spawn pid", pid);
  return Result<uint32_t>::Ok(pid);
}

Result<uint32_t> WaitFor(uint32_t pid, int32_t& status) noexcept {
  if (!g_ready) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  return WaitInternal(true, 0, pid, status);
}

Result<uint32_t> WaitChild(uint32_t parent_pid, uint32_t pid,
                           int32_t& status) noexcept {
  if (!g_ready) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  return WaitInternal(false, parent_pid, pid, status);
}

void Exit(int32_t status) noexcept {
  Process* process = Current();
  if (process != nullptr) {
    g_lock.lock();
    process->exit_code = status;
    process->state = ProcessState::kExited;
    g_lock.unlock();
    sched::WaitQueueWakeAll(g_waiters);
  }
  sched::ThreadExit();
}

Process* Current() noexcept {
  sched::Thread* thread = sched::CurrentThread();
  if (thread == nullptr) {
    return nullptr;
  }
  return static_cast<Process*>(thread->process);
}

Status SetCwd(Process& process, const char* path) noexcept {
  if (path == nullptr || path[0] != '/') {
    return Status::kInvalidArg;
  }
  const std::size_t length = mem::Length(path);
  if (length >= fs::kMaxPath) {
    return Status::kInvalidArg;
  }
  mem::Copy(process.cwd, path, length + 1U);
  return Status::kOk;
}

uint32_t Count() noexcept {
  g_lock.lock();
  uint32_t count = 0;
  for (uint32_t index = 0; index < kMaxProcesses; ++index) {
    if (g_processes[index].used != 0U) {
      ++count;
    }
  }
  g_lock.unlock();
  return count;
}

}  // namespace og::process

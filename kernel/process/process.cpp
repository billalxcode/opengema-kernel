#include "og/process/process.h"

#include "memory.h"
#include "og/arch/spinlock.h"
#include "og/arch/user.h"
#include "og/core/log.h"
#include "og/core/trace.h"
#include "og/process/elf.h"
#include "og/process/loader.h"
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

constexpr uint32_t kMaxEnv = 4;
constexpr uint64_t kMaxEnvLength = 96;
constexpr uint64_t kMaxInterpreterPath = 128;

[[nodiscard]] uint32_t FormatHex(char* out, uint64_t value) noexcept {
  constexpr char kDigits[] = "0123456789abcdef";
  char reversed[16]{};
  uint32_t count = 0;
  do {
    reversed[count] = kDigits[value & 0xFULL];
    ++count;
    value >>= 4U;
  } while (value != 0U);
  for (uint32_t index = 0; index < count; ++index) {
    out[index] = reversed[count - 1U - index];
  }
  return count;
}

void FormatEnv(char* out, const char* key, uint64_t value) noexcept {
  uint32_t cursor = 0;
  while (key[cursor] != '\0') {
    out[cursor] = key[cursor];
    ++cursor;
  }
  out[cursor] = '=';
  ++cursor;
  out[cursor] = '0';
  ++cursor;
  out[cursor] = 'x';
  ++cursor;
  cursor += FormatHex(out + cursor, value);
  out[cursor] = '\0';
}

struct InterpreterEnvironment {
  const char* values[kMaxEnv];
  uint32_t count;
  char entry[kMaxEnvLength];
  char dynamic[kMaxEnvLength];
};

void BuildInterpreterEnvironment(const elf::Image& image, bool dynamic,
                                 InterpreterEnvironment& out) noexcept {
  out.count = 0;
  if (!dynamic) {
    return;
  }
  FormatEnv(out.entry, "GEM_ENTRY", image.entry);
  FormatEnv(out.dynamic, "GEM_DYNAMIC", image.dynamic_address);
  out.values[out.count] = out.entry;
  ++out.count;
  out.values[out.count] = out.dynamic;
  ++out.count;
}

[[nodiscard]] Status BuildUserStack(const mem::AddressSpace& space, const char* path,
                                    const char* const* argv, uint32_t argc,
                                    const char* const* env, uint32_t env_count,
                                    UserStack& out) noexcept {
  if (argc > kMaxArgs || env_count > kMaxEnv) {
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
  uint64_t pointers[kMaxArgs]{};
  uint64_t environment[kMaxEnv]{};

  for (uint32_t index = 0; index < env_count; ++index) {
    if (env == nullptr || env[index] == nullptr) {
      return Status::kInvalidArg;
    }
    const uint64_t length = mem::Length(env[index]);
    if (length > kMaxEnvLength || (length + 1U) >= (cursor - kUserStackVa)) {
      return Status::kInvalidArg;
    }
    cursor -= (length + 1U);
    const Status copied =
      mem::VmmCopyOut(space, cursor, env[index], length + 1U);
    if (IsErr(copied)) {
      return copied;
    }
    environment[index] = cursor;
  }

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
  const uint64_t env_bytes =
    (static_cast<uint64_t>(env_count) + 1U) * sizeof(uint64_t);
  cursor -= env_bytes;
  uint64_t env_array[kMaxEnv + 1U]{};
  for (uint32_t index = 0; index < env_count; ++index) {
    env_array[index] = environment[index];
  }
  Status status = mem::VmmCopyOut(space, cursor, env_array, env_bytes);
  if (IsErr(status)) {
    return status;
  }
  out.envp = cursor;

  const uint64_t argv_bytes =
    (static_cast<uint64_t>(count) + 1U) * sizeof(uint64_t);
  cursor -= argv_bytes;
  uint64_t arg_array[kMaxArgs + 1U]{};
  for (uint32_t index = 0; index < count; ++index) {
    arg_array[index] = pointers[index];
  }
  status = mem::VmmCopyOut(space, cursor, arg_array, argv_bytes);
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
      ReleaseMounts(*target);
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

struct LoadedProgram {
  mem::AddressSpace space;
  elf::Image image;
  elf::Image interpreter_image;
  uint64_t file_size;
  bool dynamic;
};

// Opens the program, loads its image and optional interpreter, then maps
// both into a fresh address space. Files are closed before returning.
[[nodiscard]] Status LoadProgram(const char* path, LoadedProgram& out) noexcept {
  out = LoadedProgram{};

  const Result<fs::Stat> stat = fs::FsStat(path);
  if (stat.is_err()) {
    return stat.status();
  }
  out.file_size = stat.value().size;
  if (out.file_size == 0U || out.file_size > kMaxProgramSize) {
    return Status::kInvalidArg;
  }

  fs::FileTable files{};
  (void)fs::FileTableInit(files);
  const Result<uint32_t> file = fs::FsOpen(files, path);
  if (file.is_err()) {
    return file.status();
  }

  Status status = loader::ReadImage(files, file.value(), out.file_size,
                                    out.image);
  if (IsErr(status)) {
    (void)fs::FsClose(files, file.value());
    return status;
  }

  out.dynamic = out.image.interpreter_size != 0U;
  char interpreter_path[kMaxInterpreterPath]{};
  uint32_t interpreter_descriptor = 0;
  bool interpreter_open = false;

  if (out.dynamic) {
    if (out.image.dynamic_address == 0U) {
      (void)fs::FsClose(files, file.value());
      return Status::kInvalidArg;
    }
    status = loader::ReadInterpreterPath(files, file.value(), out.image,
                                         interpreter_path,
                                         sizeof(interpreter_path));
    if (IsErr(status)) {
      (void)fs::FsClose(files, file.value());
      return status;
    }

    const Result<fs::Stat> interpreter_stat = fs::FsStat(interpreter_path);
    if (interpreter_stat.is_err() || interpreter_stat.value().size == 0U ||
        interpreter_stat.value().size > kMaxProgramSize) {
      (void)fs::FsClose(files, file.value());
      return interpreter_stat.is_err() ? interpreter_stat.status()
                                       : Status::kInvalidArg;
    }
    const Result<uint32_t> interpreter = fs::FsOpen(files, interpreter_path);
    if (interpreter.is_err()) {
      (void)fs::FsClose(files, file.value());
      return interpreter.status();
    }
    interpreter_descriptor = interpreter.value();
    interpreter_open = true;
    status = loader::ReadImage(files, interpreter_descriptor,
                               interpreter_stat.value().size,
                               out.interpreter_image);
    if (IsErr(status)) {
      (void)fs::FsClose(files, interpreter_descriptor);
      (void)fs::FsClose(files, file.value());
      return status;
    }
    if (out.interpreter_image.interpreter_size != 0U ||
        out.interpreter_image.dynamic_address != 0U) {
      (void)fs::FsClose(files, interpreter_descriptor);
      (void)fs::FsClose(files, file.value());
      return Status::kUnsupported;
    }
  }

  status = mem::AddressSpaceInit(out.space);
  if (IsErr(status)) {
    if (interpreter_open) {
      (void)fs::FsClose(files, interpreter_descriptor);
    }
    (void)fs::FsClose(files, file.value());
    return status;
  }

  status = loader::MapImage(files, file.value(), out.image, out.space);
  if (IsOk(status) && out.dynamic) {
    status = loader::MapImage(files, interpreter_descriptor,
                              out.interpreter_image, out.space);
  }
  if (interpreter_open) {
    (void)fs::FsClose(files, interpreter_descriptor);
  }
  (void)fs::FsClose(files, file.value());
  if (IsErr(status)) {
    (void)mem::AddressSpaceDestroy(out.space);
    return status;
  }
  return Status::kOk;
}

[[nodiscard]] Result<uint32_t> CreateProcess(const char* path,
                                             uint32_t parent_pid,
                                             LoadedProgram& program,
                                             const UserStack& user_stack) noexcept {
  const uint64_t entry = program.dynamic ? program.interpreter_image.entry
                                         : program.image.entry;

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
    (void)mem::AddressSpaceDestroy(program.space);
    return Result<uint32_t>::Err(Status::kNoMem);
  }

  mem::Zero(slot, sizeof(*slot));
  uint32_t parent_uid = 0;
  uint32_t parent_gid = 0;
  if (parent_pid != kKernelPid) {
    for (uint32_t index = 0; index < kMaxProcesses; ++index) {
      if (g_processes[index].used != 0U &&
          g_processes[index].pid == parent_pid) {
        parent_uid = g_processes[index].uid;
        parent_gid = g_processes[index].gid;
        break;
      }
    }
  }
  slot->pid = NextPidLocked();
  slot->parent_pid = parent_pid;
  slot->uid = parent_uid;
  slot->gid = parent_gid;
  slot->state = ProcessState::kReady;
  slot->exit_code = 0;
  slot->used = 1;
  slot->space = program.space;
  slot->entry = entry;
  slot->stack_top = user_stack.stack_pointer;
  slot->argv = user_stack.argv;
  slot->envp = user_stack.envp;
  slot->argc = user_stack.argc;
  (void)fs::FileTableInit(slot->files);
  slot->files.uid = parent_uid;
  slot->files.gid = parent_gid;
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
    (void)mem::AddressSpaceDestroy(program.space);
    return Result<uint32_t>::Err(thread.status());
  }
  slot->thread = thread.value();
  const uint32_t pid = slot->pid;
  g_lock.unlock();
  trace::Record(trace::Tag::kSpawn, pid);
  return Result<uint32_t>::Ok(pid);
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

  LoadedProgram program{};
  const Status load_status = LoadProgram(path, program);
  if (IsErr(load_status)) {
    return Result<uint32_t>::Err(load_status);
  }

  Status status =
    mem::VmmMapUser(program.space, kUserStackVa, kUserStackSize, false);
  InterpreterEnvironment environment{};
  BuildInterpreterEnvironment(program.image, program.dynamic, environment);
  UserStack user_stack{};
  if (IsOk(status)) {
    status = BuildUserStack(program.space, path, argv, argc,
                            environment.values, environment.count, user_stack);
  }
  if (IsErr(status)) {
    (void)mem::AddressSpaceDestroy(program.space);
    return Result<uint32_t>::Err(status);
  }

  const Result<uint32_t> created =
    CreateProcess(path, parent_pid, program, user_stack);
  if (created.is_err()) {
    return created;
  }

  LogDec(LogLevel::kInfo, "user: program bytes", program.file_size);
  LogDec(LogLevel::kInfo, "user: spawn pid", created.value());
  return created;
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
    trace::Record(trace::Tag::kExit, process->pid);
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

Status SetUid(Process& process, uint32_t uid) noexcept {
  process.uid = uid;
  process.files.uid = uid;
  return Status::kOk;
}

Status SetGid(Process& process, uint32_t gid) noexcept {
  process.gid = gid;
  process.files.gid = gid;
  return Status::kOk;
}

Status RecordMount(Process& process, const char* path) noexcept {
  if (path == nullptr || path[0] != '/') {
    return Status::kInvalidArg;
  }
  if (process.mount_count >= kMaxProcessMounts) {
    return Status::kNoMem;
  }
  const std::size_t length = mem::Length(path);
  if (length >= fs::kMaxPath) {
    return Status::kInvalidArg;
  }
  char (&slot)[fs::kMaxPath] = process.mounts[process.mount_count];
  mem::Copy(slot, path, length + 1U);
  ++process.mount_count;
  return Status::kOk;
}

void ForgetMount(Process& process, const char* path) noexcept {
  if (path == nullptr) {
    return;
  }
  for (uint32_t index = process.mount_count; index > 0U; --index) {
    if (!mem::Same(process.mounts[index - 1U], path)) {
      continue;
    }
    for (uint32_t current = index - 1U; (current + 1U) < process.mount_count;
         ++current) {
      mem::Copy(process.mounts[current], process.mounts[current + 1U],
                fs::kMaxPath);
    }
    --process.mount_count;
    process.mounts[process.mount_count][0] = '\0';
    return;
  }
}

bool HasMount(const Process& process, const char* path) noexcept {
  if (path == nullptr) {
    return false;
  }
  for (uint32_t index = 0; index < process.mount_count; ++index) {
    if (mem::Same(process.mounts[index], path)) {
      return true;
    }
  }
  return false;
}

void ReleaseMounts(Process& process) noexcept {
  while (process.mount_count > 0U) {
    --process.mount_count;
    (void)fs::FsUnmount(process.mounts[process.mount_count]);
    process.mounts[process.mount_count][0] = '\0';
  }
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

uint32_t Snapshot(ProcessInfo* out, uint32_t capacity) noexcept {
  if (out == nullptr || capacity == 0U) {
    return 0;
  }
  uint32_t count = 0;
  g_lock.lock();
  for (uint32_t index = 0; index < kMaxProcesses && count < capacity; ++index) {
    const Process& process = g_processes[index];
    if (process.used == 0U) {
      continue;
    }
    out[count].pid = process.pid;
    out[count].parent_pid = process.parent_pid;
    out[count].state = process.state;
    out[count].cpu_ms =
      process.thread == nullptr ? 0U : process.thread->cpu_ticks;
    out[count].regions = process.space.region_count;
    mem::Copy(out[count].name, process.name, sizeof(out[count].name));
    out[count].name[sizeof(out[count].name) - 1U] = '\0';
    ++count;
  }
  g_lock.unlock();
  return count;
}

}  // namespace og::process

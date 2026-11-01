#include "og/process/process.h"

#include "memory.h"
#include "og/arch/spinlock.h"
#include "og/arch/user.h"
#include "bit.h"
#include "og/core/log.h"
#include "og/mem/heap.h"
#include "og/process/elf.h"
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

[[nodiscard]] Status ReadImage(fs::FileTable& files, uint32_t file,
                               uint64_t file_size, elf::Image& image) noexcept {
  uint8_t header[elf::kHeaderBufferSize]{};
  const uint64_t size =
    file_size < sizeof(header) ? file_size : static_cast<uint64_t>(sizeof(header));
  Status status = fs::FsSeek(files, file, 0);
  if (IsErr(status)) {
    return status;
  }
  uint64_t read = 0;
  status = fs::FsRead(files, file, std::span<uint8_t>(header, size), read);
  if (IsErr(status) || read != size) {
    return Status::kIoError;
  }
  const Result<elf::Image> parsed =
    elf::Parse(std::span<const uint8_t>(header, read), file_size);
  if (parsed.is_err()) {
    return parsed.status();
  }
  image = parsed.value();
  return Status::kOk;
}

[[nodiscard]] Status MapImage(fs::FileTable& files, uint32_t file,
                              const elf::Image& image,
                              mem::AddressSpace& space) noexcept {
  Status status = Status::kOk;
  for (uint32_t index = 0;
       index < image.segment_count && IsOk(status); ++index) {
    const elf::Segment& segment = image.segments[index];
    const uint64_t page_offset = segment.virtual_address & (elf::kPageSize - 1U);
    const uint64_t map_base = segment.virtual_address - page_offset;
    const uint64_t map_size =
      bits::AlignUp(page_offset + segment.memory_size, elf::kPageSize);
    status = mem::VmmMapUser(space, map_base, map_size, segment.executable);
    if (IsErr(status) || segment.file_size == 0U) {
      continue;
    }

    const Result<void*> buffer = mem::HeapAlloc(segment.file_size);
    if (buffer.is_err()) {
      status = buffer.status();
      break;
    }
    status = fs::FsSeek(files, file, segment.file_offset);
    uint64_t read = 0;
    if (IsOk(status)) {
      status = fs::FsRead(
        files, file,
        std::span<uint8_t>(static_cast<uint8_t*>(buffer.value()),
                           segment.file_size),
        read);
    }
    if (IsOk(status) && read != segment.file_size) {
      status = Status::kIoError;
    }
    if (IsOk(status)) {
      status = mem::VmmCopyOut(space, segment.virtual_address, buffer.value(),
                               segment.file_size);
    }
    (void)mem::HeapFree(buffer.value());
  }
  return status;
}

[[nodiscard]] Status ReadInterpreterPath(fs::FileTable& files, uint32_t file,
                                         const elf::Image& image, char* path,
                                         uint64_t capacity) noexcept {
  if (image.interpreter_size < 2U || image.interpreter_size > capacity) {
    return Status::kInvalidArg;
  }
  Status status = fs::FsSeek(files, file, image.interpreter_offset);
  if (IsErr(status)) {
    return status;
  }
  uint64_t read = 0;
  status = fs::FsRead(
    files, file,
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(path),
                       image.interpreter_size),
    read);
  if (IsErr(status) || read != image.interpreter_size) {
    return Status::kIoError;
  }
  if (path[image.interpreter_size - 1U] != '\0' || path[0] != '/') {
    return Status::kInvalidArg;
  }
  return Status::kOk;
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

  fs::FileTable loader_files{};
  (void)fs::FileTableInit(loader_files);
  const Result<uint32_t> file = fs::FsOpen(loader_files, path);
  if (file.is_err()) {
    return Result<uint32_t>::Err(file.status());
  }

  elf::Image image{};
  Status status = ReadImage(loader_files, file.value(), size, image);
  if (IsErr(status)) {
    (void)fs::FsClose(loader_files, file.value());
    return Result<uint32_t>::Err(status);
  }

  const bool dynamic = image.interpreter_size != 0U;
  char interpreter_path[kMaxInterpreterPath]{};
  elf::Image interpreter_image{};
  uint32_t interpreter_descriptor = 0;
  bool interpreter_open = false;

  if (dynamic) {
    if (image.dynamic_address == 0U) {
      (void)fs::FsClose(loader_files, file.value());
      return Result<uint32_t>::Err(Status::kInvalidArg);
    }
    status = ReadInterpreterPath(loader_files, file.value(), image,
                                 interpreter_path, sizeof(interpreter_path));
    if (IsErr(status)) {
      (void)fs::FsClose(loader_files, file.value());
      return Result<uint32_t>::Err(status);
    }

    const Result<fs::Stat> interpreter_stat = fs::FsStat(interpreter_path);
    if (interpreter_stat.is_err() || interpreter_stat.value().size == 0U ||
        interpreter_stat.value().size > kMaxProgramSize) {
      (void)fs::FsClose(loader_files, file.value());
      return Result<uint32_t>::Err(interpreter_stat.is_err()
                                     ? interpreter_stat.status()
                                     : Status::kInvalidArg);
    }
    const Result<uint32_t> interpreter =
      fs::FsOpen(loader_files, interpreter_path);
    if (interpreter.is_err()) {
      (void)fs::FsClose(loader_files, file.value());
      return Result<uint32_t>::Err(interpreter.status());
    }
    interpreter_descriptor = interpreter.value();
    interpreter_open = true;
    status = ReadImage(loader_files, interpreter_descriptor,
                       interpreter_stat.value().size, interpreter_image);
    if (IsErr(status)) {
      (void)fs::FsClose(loader_files, interpreter_descriptor);
      (void)fs::FsClose(loader_files, file.value());
      return Result<uint32_t>::Err(status);
    }
    if (interpreter_image.interpreter_size != 0U ||
        interpreter_image.dynamic_address != 0U) {
      (void)fs::FsClose(loader_files, interpreter_descriptor);
      (void)fs::FsClose(loader_files, file.value());
      return Result<uint32_t>::Err(Status::kUnsupported);
    }
  }

  mem::AddressSpace space{};
  status = mem::AddressSpaceInit(space);
  if (IsErr(status)) {
    if (interpreter_open) {
      (void)fs::FsClose(loader_files, interpreter_descriptor);
    }
    (void)fs::FsClose(loader_files, file.value());
    return Result<uint32_t>::Err(status);
  }

  status = MapImage(loader_files, file.value(), image, space);
  if (IsOk(status) && dynamic) {
    status = MapImage(loader_files, interpreter_descriptor, interpreter_image,
                      space);
  }
  if (interpreter_open) {
    (void)fs::FsClose(loader_files, interpreter_descriptor);
  }
  (void)fs::FsClose(loader_files, file.value());
  if (IsErr(status)) {
    (void)mem::AddressSpaceDestroy(space);
    return Result<uint32_t>::Err(status);
  }

  status = mem::VmmMapUser(space, kUserStackVa, kUserStackSize, false);
  const char* environment[kMaxEnv]{};
  uint32_t environment_count = 0;
  char entry_env[kMaxEnvLength]{};
  char dynamic_env[kMaxEnvLength]{};
  if (dynamic) {
    FormatEnv(entry_env, "GEM_ENTRY", image.entry);
    FormatEnv(dynamic_env, "GEM_DYNAMIC", image.dynamic_address);
    environment[environment_count] = entry_env;
    ++environment_count;
    environment[environment_count] = dynamic_env;
    ++environment_count;
  }
  UserStack user_stack{};
  if (IsOk(status)) {
    status = BuildUserStack(space, path, argv, argc, environment,
                            environment_count, user_stack);
  }
  if (IsErr(status)) {
    (void)mem::AddressSpaceDestroy(space);
    return Result<uint32_t>::Err(status);
  }

  const uint64_t entry = dynamic ? interpreter_image.entry : image.entry;

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
  slot->entry = entry;
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

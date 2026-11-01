#pragma once

// Canonical owner of the process object. A process owns one address space,
// one file table, and at least one thread. Kernel threads do not have a
// process object.

#include <cstdint>

#include "og/core/status.h"
#include "og/fs/vfs.h"
#include "og/mem/vmm.h"

namespace og::sched {
struct Thread;
}

namespace og::process {

constexpr uint32_t kMaxProcesses = 16;
constexpr uint32_t kKernelPid = 0;
constexpr uint64_t kUserStackVa = mem::kUserSpaceBase + 0x100000ULL;
constexpr uint64_t kUserStackSize = 16384;
constexpr uint64_t kMaxProgramSize = 65536;
constexpr uint32_t kMaxArgs = 8;
constexpr uint64_t kMaxArgLength = 64;

enum class ProcessState : uint32_t {
  kReady = 0,
  kExited = 1,
  kReaping = 2,
};

struct Process {
  uint32_t pid;
  uint32_t parent_pid;
  ProcessState state;
  int32_t exit_code;
  uint32_t used;
  mem::AddressSpace space;
  fs::FileTable files;
  sched::Thread* thread;
  uint64_t entry;
  uint64_t stack_top;
  uint64_t argv;
  uint64_t envp;
  uint32_t argc;
  char name[32];
  char cwd[fs::kMaxPath];
};

[[nodiscard]] Status Init() noexcept;

// Loads a static ELF64 executable from path and starts it in a new address
// space. With no argv, argv[0] is set to path.
[[nodiscard]] Result<uint32_t> Spawn(const char* path, uint32_t parent_pid) noexcept;

[[nodiscard]] Result<uint32_t> SpawnWithArgs(const char* path, uint32_t parent_pid,
                                             const char* const* argv,
                                             uint32_t argc) noexcept;

// Waits for any process with this pid and reaps it.
[[nodiscard]] Result<uint32_t> WaitFor(uint32_t pid, int32_t& status) noexcept;

// Waits for a child of parent_pid. pid 0 means any child.
[[nodiscard]] Result<uint32_t> WaitChild(uint32_t parent_pid, uint32_t pid,
                                         int32_t& status) noexcept;

[[noreturn]] void Exit(int32_t status) noexcept;

[[nodiscard]] Process* Current() noexcept;
[[nodiscard]] Status SetCwd(Process& process, const char* path) noexcept;
[[nodiscard]] uint32_t Count() noexcept;

}  // namespace og::process

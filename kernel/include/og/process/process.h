#pragma once

// Canonical owner of the process object. A process owns one address space,
// one file table, and at least one thread. Kernel threads do not have a
// process object.

#include <cstdint>

#include "og/arch/trap.h"
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
constexpr uint64_t kUserStackRandomRange = 8ULL * 1024U * 1024U;
constexpr uint64_t kUserStackGrowth = 48ULL * 1024U;
constexpr uint64_t kMaxProgramSize = 65536;
constexpr uint32_t kMaxArgs = 8;
constexpr uint64_t kMaxArgLength = 64;
constexpr uint32_t kMaxProcessMounts = 4;

enum class ProcessState : uint32_t {
  kReady = 0,
  kExited = 1,
  kReaping = 2,
};

struct ForkContext {
  uint64_t x[31];
  uint64_t elr;
  uint64_t spsr;
  uint64_t sp;
  bool active;
};

struct Process {
  uint32_t pid;
  uint32_t parent_pid;
  uint32_t uid;
  uint32_t gid;
  ProcessState state;
  int32_t exit_code;
  uint32_t used;
  mem::AddressSpace space;
  fs::FileTable files;
  sched::Thread* thread;
  ForkContext fork;
  uint64_t entry;
  uint64_t stack_base;
  uint64_t stack_low;
  uint64_t stack_limit;
  uint64_t stack_top;
  uint64_t argv;
  uint64_t envp;
  uint32_t argc;
  char name[32];
  char cwd[fs::kMaxPath];
  uint32_t mount_count;
  char mounts[kMaxProcessMounts][fs::kMaxPath];
};

// Copy of the fields a user visible process listing needs.
struct ProcessInfo {
  uint32_t pid;
  uint32_t parent_pid;
  uint32_t uid;
  uint32_t gid;
  ProcessState state;
  uint32_t priority;
  uint64_t cpu_ms;
  uint64_t memory_bytes;
  uint64_t memory_limit;
  uint32_t regions;
  char name[32];
};

[[nodiscard]] Status Init() noexcept;

// Loads a static ELF64 executable from path and starts it in a new address
// space. With no argv, argv[0] is set to path.
[[nodiscard]] Result<uint32_t> Spawn(const char* path, uint32_t parent_pid) noexcept;

[[nodiscard]] Result<uint32_t> SpawnWithArgs(const char* path, uint32_t parent_pid,
                                             const char* const* argv,
                                             uint32_t argc) noexcept;

// Clones the current process and resumes both at the same EL0 instruction.
// The child returns 0 from the fork syscall; the parent returns the child pid.
[[nodiscard]] Result<uint32_t> Fork(arch::aarch64::TrapFrame* frame) noexcept;

// Replaces the current process image. On success the trap frame is rewritten
// to enter the new EL0 image and this does not return to the old program.
[[nodiscard]] Status Exec(const char* path, const char* const* argv,
                          uint32_t argc,
                          arch::aarch64::TrapFrame* frame) noexcept;

// Waits for any process with this pid and reaps it.
[[nodiscard]] Result<uint32_t> WaitFor(uint32_t pid, int32_t& status) noexcept;

// Waits for a child of parent_pid. pid 0 means any child.
[[nodiscard]] Result<uint32_t> WaitChild(uint32_t parent_pid, uint32_t pid,
                                         int32_t& status) noexcept;

[[noreturn]] void Exit(int32_t status) noexcept;

// Kills the requesting process when an allocation fails with the global
// free-frame count below the OOM reserve.
[[noreturn]] void OutOfMemory(Process& process) noexcept;

// Maps one page below the current stack when fault_address falls in the
// guard-adjacent window. Returns false when the fault is not growable.
[[nodiscard]] bool GrowStack(Process& process, uint64_t fault_address) noexcept;

[[nodiscard]] Process* Current() noexcept;
[[nodiscard]] Status SetCwd(Process& process, const char* path) noexcept;
[[nodiscard]] Status SetUid(Process& process, uint32_t uid) noexcept;
[[nodiscard]] Status SetGid(Process& process, uint32_t gid) noexcept;

// Per-process record of explicitly mounted paths. Each recorded path holds
// one reference in the global mount table.
[[nodiscard]] Status RecordMount(Process& process, const char* path) noexcept;
void ForgetMount(Process& process, const char* path) noexcept;
[[nodiscard]] bool HasMount(const Process& process, const char* path) noexcept;
void ReleaseMounts(Process& process) noexcept;
[[nodiscard]] uint32_t Count() noexcept;

// Copies used process entries into out. Returns the number copied.
[[nodiscard]] uint32_t Snapshot(ProcessInfo* out, uint32_t capacity) noexcept;

}  // namespace og::process

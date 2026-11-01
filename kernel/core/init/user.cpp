#include "boot.h"

#include "og/core/log.h"
#include "og/core/panic.h"
#include "og/mem/heap.h"
#include "og/mem/pmm.h"
#include "og/process/process.h"
#include "og/syscall/syscall.h"

namespace og::boot {

void RunUserSelfTest() noexcept {
  syscall::SyscallInit();

  // gemabox uses the 2048 byte slab. Warm two blocks so concurrent spawns
  // do not grow the heap after the frame baseline is recorded.
  const Result<void*> warm_a = mem::HeapAlloc(2048);
  if (warm_a.is_err()) {
    Panic("user: heap warm a failed");
  }
  const Result<void*> warm_b = mem::HeapAlloc(2048);
  if (warm_b.is_err()) {
    (void)mem::HeapFree(warm_a.value());
    Panic("user: heap warm b failed");
  }
  (void)mem::HeapFree(warm_a.value());
  (void)mem::HeapFree(warm_b.value());

  // A text file must not be accepted as an executable. This exercises the
  // negative path of the ELF loader before the frame baseline is recorded.
  const Result<uint32_t> rejected =
    process::Spawn("/ram/hello", process::kKernelPid);
  if (rejected.is_ok()) {
    Panic("user: non-elf spawn accepted");
  }

  const uint64_t frames_before = mem::PmmFreeFrames();
  const Result<uint32_t> first =
    process::Spawn("/ram/gemabox", process::kKernelPid);
  if (first.is_err()) {
    LogDec(LogLevel::kError, "user: first spawn status",
           static_cast<uint64_t>(first.status()));
    Panic("user: first spawn failed");
  }
  const Result<uint32_t> second =
    process::Spawn("/ram/gemabox", process::kKernelPid);
  if (second.is_err()) {
    LogDec(LogLevel::kError, "user: second spawn status",
           static_cast<uint64_t>(second.status()));
    Panic("user: second spawn failed");
  }
  const char* writer_argv[] = {"gemabox", "writer"};
  const Result<uint32_t> writer =
    process::SpawnWithArgs("/ram/gemabox", process::kKernelPid, writer_argv, 2);
  if (writer.is_err()) {
    LogDec(LogLevel::kError, "user: writer spawn status",
           static_cast<uint64_t>(writer.status()));
    Panic("user: writer spawn failed");
  }
  const char* shell_argv[] = {"gemabox", "shell"};
  const Result<uint32_t> shell =
    process::SpawnWithArgs("/ram/gemabox", process::kKernelPid, shell_argv, 2);
  if (shell.is_err()) {
    LogDec(LogLevel::kError, "user: shell spawn status",
           static_cast<uint64_t>(shell.status()));
    Panic("user: shell spawn failed");
  }

  int32_t exit_code = 0;
  Result<uint32_t> waited = process::WaitFor(first.value(), exit_code);
  if (waited.is_err()) {
    Panic("user: first wait failed");
  }
  LogDec(LogLevel::kInfo, "user: exit pid", waited.value());
  LogDec(LogLevel::kInfo, "user: exit status", static_cast<uint64_t>(exit_code));

  exit_code = 0;
  waited = process::WaitFor(second.value(), exit_code);
  if (waited.is_err()) {
    Panic("user: second wait failed");
  }
  LogDec(LogLevel::kInfo, "user: exit pid", waited.value());
  LogDec(LogLevel::kInfo, "user: exit status", static_cast<uint64_t>(exit_code));

  exit_code = 0;
  waited = process::WaitFor(writer.value(), exit_code);
  if (waited.is_err()) {
    Panic("user: writer wait failed");
  }
  LogDec(LogLevel::kInfo, "user: exit pid", waited.value());
  LogDec(LogLevel::kInfo, "user: exit status", static_cast<uint64_t>(exit_code));

  exit_code = 0;
  waited = process::WaitFor(shell.value(), exit_code);
  if (waited.is_err()) {
    Panic("user: shell wait failed");
  }
  LogDec(LogLevel::kInfo, "user: exit pid", waited.value());
  LogDec(LogLevel::kInfo, "user: exit status", static_cast<uint64_t>(exit_code));

  const uint64_t frames_after = mem::PmmFreeFrames();
  LogDec(LogLevel::kInfo, "user: frames before", frames_before);
  LogDec(LogLevel::kInfo, "user: frames after", frames_after);
  LogDec(LogLevel::kInfo, "process: remaining", process::Count());
  if (frames_after != frames_before) {
    Panic("process: frames leaked");
  }
  if (process::Count() != 0U) {
    Panic("process: process leaked");
  }
}

}  // namespace og::boot

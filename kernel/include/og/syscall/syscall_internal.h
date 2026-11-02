#pragma once

// Shared declarations for syscall handler categories. Private to the
// kernel; not part of the user ABI.

#include <cstdint>

#include "og/core/status.h"
#include "og/mem/vmm.h"
#include "og/process/process.h"

namespace og::syscall::detail {

constexpr uint64_t kMaxWriteChunk = 256;
constexpr uint64_t kMaxReadChunk = 256;
constexpr uint64_t kTtyLine = 128;
constexpr uint64_t kStdout = 1;
constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kMmapFlagExecutable = 1;
constexpr uint64_t kUserMmapLimit =
  mem::kUserSpaceBase + mem::kUserSpaceSize;

[[nodiscard]] process::Process* CurrentProcess() noexcept;

[[nodiscard]] Status CopyUserString(const mem::AddressSpace& space,
                                    uint64_t user_address, char* destination,
                                    uint64_t capacity) noexcept;

[[nodiscard]] Status CopyUserPath(process::Process& process,
                                  uint64_t user_path,
                                  char* destination) noexcept;

struct UserStat {
  uint64_t type;
  uint64_t size;
};

struct UserDirEntry {
  char name[64];
  uint64_t type;
  uint64_t size;
};

[[nodiscard]] Status TtyReadLine(const mem::AddressSpace& space,
                                 uint64_t user_buffer, uint64_t length,
                                 uint64_t& result) noexcept;

}  // namespace og::syscall::detail

namespace og::syscall {

[[nodiscard]] Status DoMmap(uint64_t size, uint64_t flags,
                            uint64_t& result) noexcept;

[[nodiscard]] Status DoMunmap(uint64_t address, uint64_t size,
                              uint64_t& result) noexcept;

[[nodiscard]] Status DoRead(uint64_t descriptor, uint64_t user_buffer,
                            uint64_t length, uint64_t& result) noexcept;

[[nodiscard]] Status DoWrite(uint64_t descriptor, uint64_t user_buffer,
                             uint64_t length, uint64_t& result) noexcept;

[[nodiscard]] Status DoOpen(uint64_t user_path, uint64_t flags,
                            uint64_t& result) noexcept;

[[nodiscard]] Status DoClose(uint64_t descriptor, uint64_t& result) noexcept;

[[nodiscard]] Status DoSeek(uint64_t descriptor, uint64_t offset,
                            uint64_t& result) noexcept;

[[nodiscard]] Status DoStat(uint64_t user_path, uint64_t user_stat,
                            uint64_t& result) noexcept;

[[nodiscard]] Status DoList(uint64_t user_path, uint64_t index,
                            uint64_t user_entry, uint64_t& result) noexcept;

[[nodiscard]] Status DoChdir(uint64_t user_path, uint64_t& result) noexcept;

[[nodiscard]] Status DoGetPid(uint64_t& result) noexcept;

[[nodiscard]] Status DoWait(uint64_t pid, uint64_t user_status,
                            uint64_t& result) noexcept;

[[nodiscard]] Status DoSpawn(uint64_t user_path, uint64_t user_argv,
                             uint64_t argc, uint64_t& result) noexcept;

[[nodiscard]] Status DoShutdown(uint64_t& result) noexcept;

[[nodiscard]] Status DoReboot(uint64_t& result) noexcept;

[[nodiscard]] Status DoTime(uint64_t& result) noexcept;

[[nodiscard]] Status DoSleep(uint64_t milliseconds, uint64_t& result) noexcept;

[[nodiscard]] Status DoMkdir(uint64_t user_path, uint64_t& result) noexcept;

[[nodiscard]] Status DoUnlink(uint64_t user_path, uint64_t& result) noexcept;

[[nodiscard]] Status DoRmdir(uint64_t user_path, uint64_t& result) noexcept;

[[nodiscard]] Status DoRename(uint64_t user_old_path, uint64_t user_new_path,
                              uint64_t& result) noexcept;

[[nodiscard]] Status DoGetLogLevel(uint64_t& result) noexcept;

[[nodiscard]] Status DoSetLogLevel(uint64_t level, uint64_t& result) noexcept;

}  // namespace og::syscall

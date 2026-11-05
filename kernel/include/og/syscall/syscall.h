#pragma once

// Canonical owner of the syscall ABI.
// svc #0, number in x8, arguments in x0..x5, result in x0.

#include <cstdint>

namespace og::syscall {

constexpr uint64_t kSysWrite = 1;
constexpr uint64_t kSysRead = 2;
constexpr uint64_t kSysExit = 3;
constexpr uint64_t kSysYield = 4;
constexpr uint64_t kSysMmap = 5;
constexpr uint64_t kSysMunmap = 6;
constexpr uint64_t kSysOpen = 7;
constexpr uint64_t kSysClose = 8;
constexpr uint64_t kSysSeek = 9;
constexpr uint64_t kSysStat = 10;
constexpr uint64_t kSysGetPid = 11;
constexpr uint64_t kSysWait = 12;
constexpr uint64_t kSysSpawn = 13;
constexpr uint64_t kSysChdir = 14;
constexpr uint64_t kSysList = 15;
constexpr uint64_t kSysShutdown = 16;
constexpr uint64_t kSysReboot = 17;
constexpr uint64_t kSysTime = 18;
constexpr uint64_t kSysSleep = 19;
constexpr uint64_t kSysMkdir = 20;
constexpr uint64_t kSysUnlink = 21;
constexpr uint64_t kSysRmdir = 22;
constexpr uint64_t kSysRename = 23;
constexpr uint64_t kSysGetLogLevel = 24;
constexpr uint64_t kSysSetLogLevel = 25;
constexpr uint64_t kSysMount = 26;
constexpr uint64_t kSysUmount = 27;
constexpr uint64_t kSysWallTime = 28;
constexpr uint64_t kSysSetWallTime = 29;
constexpr uint64_t kSysLocalTime = 30;
constexpr uint64_t kSysSetTimeZone = 31;
constexpr uint64_t kSysGetTimeZone = 32;
constexpr uint64_t kSysChmod = 33;
constexpr uint64_t kSysGetUid = 34;
constexpr uint64_t kSysSetUid = 35;
constexpr uint64_t kSysGetGid = 36;
constexpr uint64_t kSysSetGid = 37;
constexpr uint64_t kSysChown = 38;
constexpr uint64_t kSysMprotect = 39;

void SyscallInit() noexcept;

}  // namespace og::syscall

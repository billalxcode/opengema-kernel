#pragma once

// Raw syscall entry points. Higher level helpers live in io.h, memory.h,
// and process.h.

#include "kgema/types.h"

namespace kgema::syscall {

constexpr i64 kWrite = 1;
constexpr i64 kRead = 2;
constexpr i64 kExit = 3;
constexpr i64 kYield = 4;
constexpr i64 kMmap = 5;
constexpr i64 kMunmap = 6;
constexpr i64 kOpen = 7;
constexpr i64 kClose = 8;
constexpr i64 kSeek = 9;
constexpr i64 kStat = 10;
constexpr i64 kGetPid = 11;
constexpr i64 kWait = 12;
constexpr i64 kSpawn = 13;
constexpr i64 kChdir = 14;
constexpr i64 kList = 15;
constexpr i64 kShutdown = 16;
constexpr i64 kReboot = 17;
constexpr i64 kTime = 18;
constexpr i64 kSleep = 19;
constexpr i64 kMkdir = 20;
constexpr i64 kUnlink = 21;
constexpr i64 kRmdir = 22;
constexpr i64 kRename = 23;
constexpr i64 kGetLogLevel = 24;
constexpr i64 kSetLogLevel = 25;
constexpr i64 kMount = 26;
constexpr i64 kUmount = 27;
constexpr i64 kWallTime = 28;
constexpr i64 kSetWallTime = 29;
constexpr i64 kLocalTime = 30;
constexpr i64 kSetTimeZone = 31;
constexpr i64 kGetTimeZone = 32;
constexpr i64 kChmod = 33;
constexpr i64 kGetUid = 34;
constexpr i64 kSetUid = 35;
constexpr i64 kGetGid = 36;
constexpr i64 kSetGid = 37;
constexpr i64 kChown = 38;
constexpr i64 kMprotect = 39;
constexpr i64 kFork = 40;
constexpr i64 kExec = 41;
constexpr i64 kGetPriority = 42;
constexpr i64 kSetPriority = 43;
constexpr i64 kFutex = 44;
constexpr i64 kGetTls = 45;
constexpr i64 kSetTls = 46;

// Negative Linux errno values returned by the kernel in x0.
constexpr i64 kErrPermission = -1;
constexpr i64 kErrNotFound = -2;
constexpr i64 kErrIo = -5;
constexpr i64 kErrNoMem = -12;
constexpr i64 kErrDenied = -13;
constexpr i64 kErrBusy = -16;
constexpr i64 kErrExists = -17;
constexpr i64 kErrNoDevice = -19;
constexpr i64 kErrInvalid = -22;
constexpr i64 kErrNoSys = -38;
constexpr i64 kErrUnsupported = -95;
constexpr i64 kErrTimeout = -110;

// Number in x8, arguments in x0..x2, result in x0.
[[nodiscard]] i64 invoke3(i64 number, i64 arg0, i64 arg1, i64 arg2) noexcept;

// Number in x8, arguments in x0..x3, result in x0.
[[nodiscard]] i64 invoke4(i64 number, i64 arg0, i64 arg1, i64 arg2,
                          i64 arg3) noexcept;

// Last negative errno observed by invoke3, or zero after success.
[[nodiscard]] i64 last_error() noexcept;

}  // namespace kgema::syscall

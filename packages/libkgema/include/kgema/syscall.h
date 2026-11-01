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

// Number in x8, arguments in x0..x2, result in x0.
[[nodiscard]] i64 invoke3(i64 number, i64 arg0, i64 arg1, i64 arg2) noexcept;

}  // namespace kgema::syscall

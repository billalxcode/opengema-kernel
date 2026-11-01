#pragma once

// Basic types for the GEMA public API. No standard library headers are used,
// so the same types work with -nostdinc.

namespace gemapi {

using usize = unsigned long;
using u64 = unsigned long;
using i64 = long;
using u32 = unsigned int;
using u8 = unsigned char;

constexpr usize kPageSize = 4096;

}  // namespace gemapi

#pragma once

// Basic types for the GEMA dynamic linker. No standard library headers.

namespace ldgem {

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long;
using i64 = long;
using usize = unsigned long;

constexpr u64 kPageSize = 4096;

}  // namespace ldgem

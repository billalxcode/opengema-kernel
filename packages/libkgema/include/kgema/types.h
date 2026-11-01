#pragma once

// Basic types for the kgema user space SDK. No standard library headers are
// used, so the same types work with -nostdinc.

namespace kgema {

using usize = unsigned long;
using u64 = unsigned long;
using i64 = long;

}  // namespace kgema

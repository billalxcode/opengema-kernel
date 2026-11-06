#include "kgema/syscall.h"

namespace kgema::syscall {

namespace {

i64 g_last_error = 0;

}  // namespace

i64 invoke3(i64 number, i64 arg0, i64 arg1, i64 arg2) noexcept {
  register i64 x0 __asm__("x0") = arg0;
  register i64 x1 __asm__("x1") = arg1;
  register i64 x2 __asm__("x2") = arg2;
  register i64 x8 __asm__("x8") = number;
  __asm__ volatile("svc #0"
                   : "+r"(x0)
                   : "r"(x1), "r"(x2), "r"(x8)
                   : "memory");
  g_last_error = x0 < 0 ? x0 : 0;
  return x0;
}

i64 last_error() noexcept {
  return g_last_error;
}

}  // namespace kgema::syscall

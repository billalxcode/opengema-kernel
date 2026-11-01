// Stack canary support.
// aarch64-windows uses the MSVC names, other targets use the GNU names.
// Both are provided; the guard value is replaced at boot.

#include <cstdint>

#include "og/panic.h"

namespace {

constexpr uintptr_t kInitialGuard = 0x9E3779B97F4A7C15ULL;

}  // namespace

extern "C" {

uintptr_t __stack_chk_guard = kInitialGuard;
uintptr_t __security_cookie = kInitialGuard;

[[noreturn]] void __stack_chk_fail() {
  og::Panic("stack canary corrupted");
}

void __security_check_cookie(uintptr_t cookie) {
  if (cookie != __security_cookie) {
    og::Panic("stack canary corrupted");
  }
}

}  // extern "C"

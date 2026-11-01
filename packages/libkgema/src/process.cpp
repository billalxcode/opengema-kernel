#include "kgema/process.h"

#include "kgema/syscall.h"

namespace kgema {

[[noreturn]] void exit(int status) noexcept {
  (void)syscall::invoke3(syscall::kExit, static_cast<i64>(status), 0, 0);
  for (;;) {
  }
}

}  // namespace kgema

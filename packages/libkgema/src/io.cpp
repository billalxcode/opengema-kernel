#include "kgema/io.h"

#include "kgema/syscall.h"

namespace kgema {

void write(const char* data, usize size) noexcept {
  if (data == nullptr || size == 0) {
    return;
  }
  (void)syscall::invoke3(syscall::kWrite, 1, reinterpret_cast<i64>(data),
                         static_cast<i64>(size));
}

}  // namespace kgema

// gemabox: base user command program for opengema-kernel.
// Built for EL0 with the kgema SDK.

#include "kgema/io.h"
#include "kgema/memory.h"
#include "kgema/types.h"

namespace {

constexpr kgema::usize kPageSize = 4096;
constexpr char kHello[] = "hello from el0\n";
constexpr char kMmapMessage[] = "mmap ok\n";
constexpr char kMmapFailed[] = "mmap failed\n";

}  // namespace

int main() {
  kgema::write(kHello);

  void* mapping = kgema::mmap(kPageSize);
  if (mapping == nullptr) {
    kgema::write(kMmapFailed);
    return 1;
  }

  auto* bytes = static_cast<char*>(mapping);
  for (kgema::usize index = 0; index < (sizeof(kMmapMessage) - 1U); ++index) {
    bytes[index] = kMmapMessage[index];
  }
  kgema::write(bytes, sizeof(kMmapMessage) - 1U);

  if (!kgema::munmap(mapping, kPageSize)) {
    return 1;
  }
  return 0;
}

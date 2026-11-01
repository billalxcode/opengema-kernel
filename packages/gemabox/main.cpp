// gemabox: base user command program for opengema-kernel.
// Built for EL0 with the kgema SDK.

#include "kgema/file.h"
#include "kgema/io.h"
#include "kgema/memory.h"
#include "kgema/process.h"
#include "kgema/types.h"

namespace {

constexpr kgema::usize kPageSize = 4096;
constexpr char kHello[] = "hello from el0\n";
constexpr char kFileOk[] = "file ok\n";
constexpr char kMmapMessage[] = "mmap ok\n";
constexpr char kMmapFailed[] = "mmap failed\n";

}  // namespace

int main() {
  if (kgema::getpid() == 0U) {
    return 1;
  }
  kgema::write(kHello);

  const kgema::i64 descriptor = kgema::open("/ram/hello");
  if (descriptor < 0) {
    return 1;
  }
  char text[32];
  const kgema::i64 count = kgema::read(descriptor, text, sizeof(text));
  kgema::Stat info{};
  bool file_ok = count >= 5;
  file_ok = file_ok && kgema::stat("/ram/hello", info);
  file_ok = file_ok && info.size >= 5U;
  file_ok = file_ok && kgema::seek(descriptor, 0);
  char first = '\0';
  file_ok = file_ok && kgema::read(descriptor, &first, 1) == 1;
  file_ok = file_ok && first == 'r';
  (void)kgema::close(descriptor);
  if (!file_ok) {
    return 1;
  }
  kgema::write(kFileOk);

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

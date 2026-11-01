// gemabox: base user command program for opengema-kernel.
// Built for EL0 against the public gemapi layer only. It never calls a
// syscall and never includes a kernel header.

#include "gemapi/error.h"
#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/memory.h"
#include "gemapi/paths.h"
#include "gemapi/process.h"
#include "gemapi/types.h"
#include "gemabox/applets.h"

namespace {

constexpr gemapi::usize kPageSize = gemapi::kPageSize;
constexpr char kHello[] = "hello from el0\n";
constexpr char kFileOk[] = "file ok\n";
constexpr char kMmapMessage[] = "mmap ok\n";
constexpr char kMmapFailed[] = "mmap failed\n";
constexpr char kChildOk[] = "child ok\n";
constexpr char kSpawnOk[] = "spawn ok\n";
// Test file under the fixed /tmp mount.
constexpr char kHelloPath[] = "/tmp/hello";

bool same(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  while (*lhs != '\0' && *lhs == *rhs) {
    ++lhs;
    ++rhs;
  }
  return *lhs == *rhs;
}

}  // namespace

int main(int argc, char** argv, char** /*envp*/) {
  if (argc > 1 && same(argv[1], "child")) {
    gemapi::io::write(kChildOk);
    return 0;
  }
  if (argc > 1 && same(argv[1], "writer")) {
    return gemabox::run_writer();
  }
  if (argc > 1 && same(argv[1], "shell")) {
    return gemabox::run_shell();
  }
  if (gemapi::process::id() == 0U) {
    return 1;
  }
  gemapi::io::write(kHello);

  auto file = gemapi::fs::File::open(kHelloPath);
  if (!file) {
    return 1;
  }
  char text[32];
  auto count = file.value().read(text, sizeof(text));
  bool file_ok = count && count.value() >= 5U;
  auto info = gemapi::fs::stat(kHelloPath);
  file_ok = file_ok && info && info.value().size >= 5U;
  file_ok = file_ok && file.value().seek(0) == gemapi::Status::ok;
  char first = '\0';
  auto one = file.value().read(&first, 1);
  file_ok = file_ok && one && one.value() == 1U && first == 'r';
  if (!file_ok) {
    return 1;
  }
  gemapi::io::write(kFileOk);

  auto mapping = gemapi::memory::map(kPageSize);
  if (!mapping) {
    gemapi::io::write(kMmapFailed);
    return 1;
  }

  auto* bytes = static_cast<char*>(mapping.value());
  for (gemapi::usize index = 0; index < (sizeof(kMmapMessage) - 1U); ++index) {
    bytes[index] = kMmapMessage[index];
  }
  gemapi::io::write(bytes, sizeof(kMmapMessage) - 1U);

  if (gemapi::memory::unmap(mapping.value(), kPageSize) != gemapi::Status::ok) {
    return 1;
  }

  const char* child_argv[] = {"gemabox", "child"};
  auto child = gemapi::process::spawn(gemapi::paths::kGemabox, child_argv, 2);
  if (!child) {
    return 1;
  }
  int child_status = 0;
  if (gemapi::process::wait(child.value(), child_status) != gemapi::Status::ok ||
      child_status != 0) {
    return 1;
  }
  gemapi::io::write(kSpawnOk);
  return 0;
}

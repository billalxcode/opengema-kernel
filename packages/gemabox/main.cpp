// gemabox: base user command program for opengema-kernel.
// Built for EL0 against the public gemapi layer only. It never calls a
// syscall and never includes a kernel header.

#include "gemapi/error.h"
#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/memory.h"
#include "gemapi/process.h"
#include "gemapi/types.h"

namespace {

constexpr gemapi::usize kPageSize = gemapi::kPageSize;
constexpr char kHello[] = "hello from el0\n";
constexpr char kFileOk[] = "file ok\n";
constexpr char kMmapMessage[] = "mmap ok\n";
constexpr char kMmapFailed[] = "mmap failed\n";
constexpr char kChildOk[] = "child ok\n";
constexpr char kSpawnOk[] = "spawn ok\n";
constexpr char kWriteMessage[] = "write ok\n";
constexpr char kPrompt[] = "gemabox> ";
constexpr char kHelp[] = "commands: help, echo, cat, exit\n";

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

bool starts_with(const char* text, const char* prefix) {
  if (text == nullptr || prefix == nullptr) {
    return false;
  }
  while (*prefix != '\0') {
    if (*text != *prefix) {
      return false;
    }
    ++text;
    ++prefix;
  }
  return true;
}

void cat_file(const char* path) {
  auto file = gemapi::fs::File::open(path);
  if (!file) {
    gemapi::io::write("cat: open failed\n");
    return;
  }
  char buffer[64];
  for (;;) {
    auto count = file.value().read(buffer, sizeof(buffer));
    if (!count) {
      gemapi::io::write("cat: read failed\n");
      break;
    }
    if (count.value() == 0U) {
      break;
    }
    gemapi::io::write(buffer, count.value());
  }
}

int run_shell() {
  char line[128];
  for (;;) {
    gemapi::io::write(kPrompt);
    auto length = gemapi::io::read_line(line, sizeof(line));
    if (!length) {
      return 1;
    }
    const gemapi::usize size = length.value();
    if (size == 0U) {
      continue;
    }
    if (same(line, "exit")) {
      return 0;
    }
    if (same(line, "help")) {
      gemapi::io::write(kHelp);
      continue;
    }
    if (starts_with(line, "echo ") && size > 5U) {
      gemapi::io::write(line + 5, size - 5U);
      gemapi::io::write("\n");
      continue;
    }
    if (starts_with(line, "cat ") && size > 4U) {
      cat_file(line + 4);
      continue;
    }
    gemapi::io::write("unknown: ");
    gemapi::io::write(line, size);
    gemapi::io::write("\n");
  }
}

int run_writer() {
  if (gemapi::fs::chdir("/tmp") != gemapi::Status::ok) {
    return 1;
  }
  auto file = gemapi::fs::File::open("writer", gemapi::fs::kOpenWrite);
  if (!file) {
    return 1;
  }
  const gemapi::usize message_size = sizeof(kWriteMessage) - 1U;
  if (file.value().write(kWriteMessage, message_size) != gemapi::Status::ok) {
    return 1;
  }
  if (file.value().seek(0) != gemapi::Status::ok) {
    return 1;
  }
  char readback[16];
  auto count = file.value().read(readback, sizeof(readback));
  if (!count || count.value() < message_size || readback[0] != 'w') {
    return 1;
  }
  gemapi::io::write(kWriteMessage);
  return 0;
}

}  // namespace

int main(int argc, char** argv, char** /*envp*/) {
  if (argc > 1 && same(argv[1], "child")) {
    gemapi::io::write(kChildOk);
    return 0;
  }
  if (argc > 1 && same(argv[1], "writer")) {
    return run_writer();
  }
  if (argc > 1 && same(argv[1], "shell")) {
    return run_shell();
  }
  if (gemapi::process::id() == 0U) {
    return 1;
  }
  gemapi::io::write(kHello);

  auto file = gemapi::fs::File::open("/tmp/hello");
  if (!file) {
    return 1;
  }
  char text[32];
  auto count = file.value().read(text, sizeof(text));
  bool file_ok = count && count.value() >= 5U;
  auto info = gemapi::fs::stat("/tmp/hello");
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
  auto child = gemapi::process::spawn("/applications/gemabox/gemabox", child_argv, 2);
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

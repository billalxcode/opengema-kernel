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
  const kgema::i64 descriptor = kgema::open(path);
  if (descriptor < 0) {
    kgema::write("cat: open failed\n");
    return;
  }
  char buffer[64];
  for (;;) {
    const kgema::i64 count = kgema::read(descriptor, buffer, sizeof(buffer));
    if (count < 0) {
      kgema::write("cat: read failed\n");
      break;
    }
    if (count == 0) {
      break;
    }
    kgema::write(buffer, static_cast<kgema::usize>(count));
  }
  (void)kgema::close(descriptor);
}

int run_shell() {
  char line[128];
  for (;;) {
    kgema::write(kPrompt);
    const kgema::i64 length = kgema::read(0, line, sizeof(line) - 1U);
    if (length < 0) {
      return 1;
    }
    line[length] = '\0';
    if (length == 0) {
      continue;
    }
    if (same(line, "exit")) {
      return 0;
    }
    if (same(line, "help")) {
      kgema::write(kHelp);
      continue;
    }
    if (starts_with(line, "echo ") && length > 5) {
      kgema::write(line + 5, static_cast<kgema::usize>(length - 5));
      kgema::write("\n");
      continue;
    }
    if (starts_with(line, "cat ") && length > 4) {
      cat_file(line + 4);
      continue;
    }
    kgema::write("unknown: ");
    kgema::write(line, static_cast<kgema::usize>(length));
    kgema::write("\n");
  }
}

int run_writer() {
  if (!kgema::chdir("/ram")) {
    return 1;
  }
  const kgema::i64 descriptor = kgema::open("writer", kgema::kOpenWrite);
  if (descriptor < 0) {
    return 1;
  }
  const kgema::i64 written =
    kgema::write(descriptor, kWriteMessage, sizeof(kWriteMessage) - 1U);
  if (written != static_cast<kgema::i64>(sizeof(kWriteMessage) - 1U)) {
    (void)kgema::close(descriptor);
    return 1;
  }
  if (!kgema::seek(descriptor, 0)) {
    (void)kgema::close(descriptor);
    return 1;
  }
  char readback[16];
  const kgema::i64 read = kgema::read(descriptor, readback, sizeof(readback));
  (void)kgema::close(descriptor);
  if (read < static_cast<kgema::i64>(sizeof(kWriteMessage) - 1U) ||
      readback[0] != 'w') {
    return 1;
  }
  kgema::write(kWriteMessage);
  return 0;
}

}  // namespace

int main(int argc, char** argv, char** /*envp*/) {
  if (argc > 1 && same(argv[1], "child")) {
    kgema::write(kChildOk);
    return 0;
  }
  if (argc > 1 && same(argv[1], "writer")) {
    return run_writer();
  }
  if (argc > 1 && same(argv[1], "shell")) {
    return run_shell();
  }
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

  const char* child_argv[] = {"gemabox", "child"};
  const kgema::i64 child = kgema::spawn("/ram/gemabox", child_argv, 2);
  if (child <= 0) {
    return 1;
  }
  int child_status = 0;
  if (kgema::wait(child, &child_status) != child || child_status != 0) {
    return 1;
  }
  kgema::write(kSpawnOk);
  return 0;
}

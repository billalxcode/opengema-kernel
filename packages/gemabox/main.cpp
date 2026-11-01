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
constexpr char kHelp[] = "commands: help, echo, cat, ls, clear, exit\n";
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

gemapi::usize text_length(const char* text) {
  gemapi::usize length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  return length;
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

void list_dir(const char* path) {
  for (gemapi::u64 index = 0;; ++index) {
    gemapi::fs::DirEntry entry{};
    auto found = gemapi::fs::list(path, index, entry);
    if (!found) {
      if (found.status() != gemapi::Status::ok) {
        gemapi::io::write("ls: list failed\n");
      }
      return;
    }
    if (!found.value()) {
      return;
    }
    gemapi::io::write(entry.name);
    if (entry.kind == gemapi::fs::Kind::directory) {
      gemapi::io::write("/");
    }
    gemapi::io::write("\n");
  }
}

bool run_path(const char* path, const char* name) {
  const char* argv[] = {name};
  auto child = gemapi::process::spawn(path, argv, 1);
  if (!child) {
    gemapi::io::write("run: spawn failed\n");
    return true;
  }
  int status = 0;
  if (gemapi::process::wait(child.value(), status) != gemapi::Status::ok) {
    gemapi::io::write("run: wait failed\n");
  }
  return true;
}

bool run_external(const char* name) {
  char path[128];
  constexpr char kCommandsPrefix[] = "/commands/";
  constexpr char kApplicationsPrefix[] = "/applications/";
  const gemapi::usize name_length = text_length(name);

  gemapi::usize cursor = sizeof(kCommandsPrefix) - 1U;
  if ((cursor + name_length + 1U) >= sizeof(path)) {
    return false;
  }
  for (gemapi::usize index = 0; index < cursor; ++index) {
    path[index] = kCommandsPrefix[index];
  }
  for (gemapi::usize index = 0; index < name_length; ++index) {
    path[cursor + index] = name[index];
  }
  path[cursor + name_length] = '\0';
  if (gemapi::fs::stat(path)) {
    return run_path(path, name);
  }

  cursor = sizeof(kApplicationsPrefix) - 1U;
  if ((cursor + name_length + 1U + name_length + 1U) >= sizeof(path)) {
    return false;
  }
  for (gemapi::usize index = 0; index < cursor; ++index) {
    path[index] = kApplicationsPrefix[index];
  }
  for (gemapi::usize index = 0; index < name_length; ++index) {
    path[cursor + index] = name[index];
  }
  cursor += name_length;
  path[cursor] = '/';
  ++cursor;
  for (gemapi::usize index = 0; index < name_length; ++index) {
    path[cursor + index] = name[index];
  }
  path[cursor + name_length] = '\0';
  if (!gemapi::fs::stat(path)) {
    return false;
  }
  return run_path(path, name);
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
    if (same(line, "ls")) {
      list_dir("/");
      continue;
    }
    if (starts_with(line, "ls ") && size > 3U) {
      list_dir(line + 3);
      continue;
    }
    if (same(line, "clear")) {
      gemapi::io::write("\x1b[2J\x1b[H");
      continue;
    }
    bool has_space = false;
    for (gemapi::usize index = 0; index < size; ++index) {
      if (line[index] == ' ') {
        has_space = true;
        break;
      }
    }
    if (!has_space && run_external(line)) {
      continue;
    }
    gemapi::io::write("unknown: ");
    gemapi::io::write(line, size);
    gemapi::io::write("\n");
  }
}

int run_writer() {
  if (gemapi::fs::chdir(gemapi::paths::kTmp) != gemapi::Status::ok) {
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

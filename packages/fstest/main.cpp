// fstest: persistence test application for the FAT32 boot volume.
// It is a test utility, not part of the normal shell command set.

#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/paths.h"
#include "gemapi/types.h"

namespace {

constexpr char kMessage[] = "persist ok\n";

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

gemapi::usize append(char* out, gemapi::usize capacity,
                     gemapi::usize cursor, const char* text) {
  for (gemapi::usize index = 0; text[index] != '\0'; ++index) {
    if (cursor >= capacity) {
      return capacity;
    }
    out[cursor] = text[index];
    ++cursor;
  }
  return cursor;
}

bool build_path(char* out, gemapi::usize capacity, const char* name) {
  gemapi::usize cursor =
    append(out, capacity, 0, gemapi::paths::kBootVolume);
  cursor = append(out, capacity, cursor, "/");
  cursor = append(out, capacity, cursor, name);
  if (cursor >= capacity) {
    return false;
  }
  out[cursor] = '\0';
  return true;
}

template <gemapi::usize N>
int write_file(const char* path, bool create, const char (&marker)[N]) {
  auto file = gemapi::fs::File::open(
    path, gemapi::fs::kOpenWrite |
            (create ? gemapi::fs::kOpenCreate : 0));
  if (!file) {
    gemapi::io::write("fstest: open failed\n");
    return 1;
  }
  if (file.value().write(kMessage, sizeof(kMessage) - 1U) !=
      gemapi::Status::ok) {
    gemapi::io::write("fstest: write failed\n");
    return 1;
  }
  gemapi::io::write(marker);
  return 0;
}

template <gemapi::usize N>
int read_file(const char* path, const char (&marker)[N]) {
  auto file = gemapi::fs::File::open(path);
  if (!file) {
    gemapi::io::write("fstest: open failed\n");
    return 1;
  }
  char buffer[16]{};
  auto count = file.value().read(buffer, sizeof(buffer));
  if (!count || count.value() < (sizeof(kMessage) - 1U) ||
      buffer[0] != 'p') {
    gemapi::io::write("fstest: read failed\n");
    return 1;
  }
  gemapi::io::write(marker);
  return 0;
}

int remove_path(const char* path) {
  const auto info = gemapi::fs::stat(path);
  if (!info) {
    gemapi::io::write("fstest: delete failed\n");
    return 1;
  }
  const gemapi::Status status =
    info.value().kind == gemapi::fs::Kind::directory
      ? gemapi::fs::rmdir(path)
      : gemapi::fs::unlink(path);
  if (status != gemapi::Status::ok) {
    gemapi::io::write("fstest: delete failed\n");
    return 1;
  }
  gemapi::io::write("fstest: delete ok\n");
  return 0;
}

}  // namespace

int main(int argc, char** argv, char** envp) {
  (void)envp;
  if (argc != 2 || argv[1] == nullptr) {
    gemapi::io::write(
      "usage: fstest write|read|create|read2|mkdir|delete|rename|"
      "truncate|empty|long|readlong\n");
    return 2;
  }

  char path[96];
  char path2[96];
  char path3[96];
  char path4[96];
  char path5[96];
  if (!build_path(path, sizeof(path), "persist.bin") ||
      !build_path(path2, sizeof(path2), "persist2.bin") ||
      !build_path(path3, sizeof(path3), "testdir") ||
      !build_path(path4, sizeof(path4), "renamed.bin") ||
      !build_path(path5, sizeof(path5), "persist long name.txt")) {
    gemapi::io::write("fstest: path too long\n");
    return 1;
  }

  const char* command = argv[1];
  if (same(command, "write")) {
    return write_file(path, false, "fstest: write ok\n");
  }
  if (same(command, "create")) {
    return write_file(path2, true, "fstest: create ok\n");
  }
  if (same(command, "read")) {
    return read_file(path, "fstest: read ok\n");
  }
  if (same(command, "read2")) {
    return read_file(path2, "fstest: read2 ok\n");
  }
  if (same(command, "mkdir")) {
    if (gemapi::fs::mkdir(path3) != gemapi::Status::ok) {
      gemapi::io::write("fstest: mkdir failed\n");
      return 1;
    }
    gemapi::io::write("fstest: mkdir ok\n");
    return 0;
  }
  if (same(command, "delete")) {
    return remove_path(path3);
  }
  if (same(command, "rename")) {
    if (gemapi::fs::rename(path2, path4) != gemapi::Status::ok) {
      gemapi::io::write("fstest: rename failed\n");
      return 1;
    }
    gemapi::io::write("fstest: rename ok\n");
    return 0;
  }
  if (same(command, "truncate")) {
    auto file = gemapi::fs::File::open(
      path, gemapi::fs::kOpenWrite | gemapi::fs::kOpenTruncate);
    const auto info = gemapi::fs::stat(path);
    if (!file || !info || info.value().size != 0U) {
      gemapi::io::write("fstest: truncate failed\n");
      return 1;
    }
    gemapi::io::write("fstest: truncate ok\n");
    return 0;
  }
  if (same(command, "empty")) {
    const auto info = gemapi::fs::stat(path);
    if (!info || info.value().size != 0U) {
      gemapi::io::write("fstest: empty failed\n");
      return 1;
    }
    gemapi::io::write("fstest: empty ok\n");
    return 0;
  }
  if (same(command, "long")) {
    return write_file(path5, true, "fstest: long ok\n");
  }
  if (same(command, "readlong")) {
    return read_file(path5, "fstest: readlong ok\n");
  }

  gemapi::io::write("fstest: unknown command\n");
  return 2;
}

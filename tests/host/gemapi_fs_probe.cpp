#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gemapi/fs.h"
#include "kgema/file.h"

namespace {

char g_written[64];
std::size_t g_written_size = 0;
bool g_closed[16] = {};

bool Same(const char* lhs, const char* rhs) {
  return lhs != nullptr && rhs != nullptr && std::strcmp(lhs, rhs) == 0;
}

}  // namespace

namespace kgema {

i64 open(const char* path, u64 /*flags*/) noexcept {
  if (Same(path, "good")) {
    return 7;
  }
  if (Same(path, "second")) {
    return 8;
  }
  return -1;
}

bool close(i64 descriptor) noexcept {
  if (descriptor < 0 || descriptor >= 16) {
    return false;
  }
  g_closed[descriptor] = true;
  return true;
}

i64 read(i64 descriptor, void* buffer, usize size) noexcept {
  if (descriptor != 7 || buffer == nullptr) {
    return -1;
  }
  const usize available = 4U;
  const usize count = size < available ? size : available;
  auto* bytes = static_cast<char*>(buffer);
  for (usize index = 0; index < count; ++index) {
    bytes[index] = static_cast<char>('a' + index);
  }
  return static_cast<i64>(count);
}

i64 write(i64 descriptor, const void* data, usize size) noexcept {
  if (descriptor != 7 || data == nullptr) {
    return -1;
  }
  usize count = size < 2U ? size : 2U;
  if ((g_written_size + count) > sizeof(g_written)) {
    return -1;
  }
  const auto* bytes = static_cast<const char*>(data);
  for (usize index = 0; index < count; ++index) {
    g_written[g_written_size + index] = bytes[index];
  }
  g_written_size += count;
  return static_cast<i64>(count);
}

bool seek(i64 descriptor, u64 offset) noexcept {
  return descriptor == 7 && offset <= 64U;
}

bool stat(const char* path, Stat& result) noexcept {
  result = Stat{};
  if (Same(path, "file")) {
    result.type = 0;
    result.size = 10;
    return true;
  }
  if (Same(path, "dir")) {
    result.type = 1;
    result.size = 20;
    return true;
  }
  if (Same(path, "dev")) {
    result.type = 2;
    result.size = 30;
    return true;
  }
  if (Same(path, "other")) {
    result.type = 9;
    result.size = 40;
    return true;
  }
  return false;
}

bool chdir(const char* path) noexcept {
  return Same(path, "dir");
}

i64 list(const char* path, u64 index, DirEntry& entry) noexcept {
  if (Same(path, "error")) {
    return -1;
  }
  if (Same(path, "good")) {
    if (index == 0) {
      entry = DirEntry{};
      std::strcpy(entry.name, "entry");
      entry.type = 1;
      entry.size = 12;
      return 1;
    }
    return 0;
  }
  if (Same(path, "long")) {
    if (index == 0) {
      entry = DirEntry{};
      for (std::size_t offset = 0; offset < 63U; ++offset) {
        entry.name[offset] = 'x';
      }
      entry.name[63] = '\0';
      entry.type = 0;
      entry.size = 5;
      return 1;
    }
    return 0;
  }
  return 0;
}

}  // namespace kgema

namespace {

int Fail(const char* message) {
  std::printf("gemapi fs probe: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  using gemapi::Status;
  using gemapi::fs::File;
  using gemapi::fs::Kind;

  auto opened = File::open("good");
  if (!opened || opened.value().descriptor() != 7) {
    return Fail("open good failed");
  }
  if (File::open("bad").status() != Status::not_found) {
    return Fail("open bad status");
  }
  if (File::open(nullptr).status() != Status::invalid_argument) {
    return Fail("open null status");
  }

  File file = static_cast<File&&>(opened.value());
  if (!file.valid() || opened.value().valid()) {
    return Fail("move constructor state");
  }
  if (opened.value().read(g_written, 1U).status() !=
      Status::invalid_argument) {
    return Fail("read moved-from handle");
  }

  char buffer[8]{};
  auto read = file.read(buffer, sizeof(buffer));
  if (!read || read.value() != 4U || buffer[0] != 'a' || buffer[3] != 'd') {
    return Fail("read failed");
  }
  if (file.read(nullptr, 1U).status() != Status::invalid_argument) {
    return Fail("read null buffer");
  }
  auto empty = file.read(buffer, 0U);
  if (!empty || empty.value() != 0U) {
    return Fail("read zero size");
  }

  const char payload[] = "abcdef";
  if (file.write(payload, 6U) != Status::ok) {
    return Fail("write failed");
  }
  if (g_written_size != 6U || !Same(g_written, "abcdef")) {
    return Fail("write loop mismatch");
  }
  if (file.write(nullptr, 1U) != Status::invalid_argument) {
    return Fail("write null buffer");
  }
  if (file.write(payload, 0U) != Status::ok) {
    return Fail("write zero size");
  }

  if (file.seek(10U) != Status::ok) {
    return Fail("seek failed");
  }
  if (file.seek(100U) != Status::io) {
    return Fail("seek bound status");
  }

  auto second = File::open("second");
  if (!second || second.value().descriptor() != 8) {
    return Fail("open second failed");
  }
  file = static_cast<File&&>(second.value());
  if (file.descriptor() != 8 || !g_closed[7] || second.value().valid()) {
    return Fail("move assignment state");
  }

  if (file.close() != Status::ok || file.valid()) {
    return Fail("close failed");
  }
  if (file.close() != Status::ok) {
    return Fail("close idempotence");
  }
  if (!g_closed[8]) {
    return Fail("close not forwarded");
  }

  const auto file_info = gemapi::fs::stat("file");
  if (!file_info || file_info.value().kind != Kind::file ||
      file_info.value().size != 10U) {
    return Fail("stat file");
  }
  if (gemapi::fs::stat("dir").value().kind != Kind::directory ||
      gemapi::fs::stat("dev").value().kind != Kind::device ||
      gemapi::fs::stat("other").value().kind != Kind::other) {
    return Fail("stat kind mapping");
  }
  if (gemapi::fs::stat("missing").status() != Status::not_found) {
    return Fail("stat missing");
  }
  if (gemapi::fs::stat(nullptr).status() != Status::invalid_argument) {
    return Fail("stat null");
  }

  if (gemapi::fs::chdir("dir") != Status::ok) {
    return Fail("chdir dir");
  }
  if (gemapi::fs::chdir("missing") != Status::not_found) {
    return Fail("chdir missing");
  }
  if (gemapi::fs::chdir(nullptr) != Status::invalid_argument) {
    return Fail("chdir null");
  }

  gemapi::fs::DirEntry entry{};
  auto listed = gemapi::fs::list("good", 0U, entry);
  if (!listed || !listed.value() || !Same(entry.name, "entry") ||
      entry.kind != Kind::directory || entry.size != 12U) {
    return Fail("list entry");
  }
  auto end = gemapi::fs::list("good", 1U, entry);
  if (!end || end.value()) {
    return Fail("list end");
  }
  if (gemapi::fs::list("error", 0U, entry).status() != Status::io) {
    return Fail("list error");
  }
  if (gemapi::fs::list(nullptr, 0U, entry).status() !=
      Status::invalid_argument) {
    return Fail("list null");
  }

  gemapi::fs::DirEntry long_entry{};
  auto long_listed = gemapi::fs::list("long", 0U, long_entry);
  if (!long_listed || !long_listed.value() ||
      std::strlen(long_entry.name) != 63U) {
    return Fail("list long name");
  }

  std::printf("gemapi fs probe: OK\n");
  return 0;
}

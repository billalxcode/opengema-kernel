#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "kgema/file.h"
#include "kgema/io.h"
#include "kgema/memory.h"
#include "kgema/process.h"
#include "kgema/syscall.h"

using kgema::i64;

namespace {

char g_written[64];
std::size_t g_written_size = 0;
i64 g_last_number = 0;
i64 g_last_arg0 = 0;
i64 g_last_arg1 = 0;
i64 g_last_arg2 = 0;
i64 g_last_arg3 = 0;

bool Same(const char* lhs, const char* rhs) {
  return lhs != nullptr && rhs != nullptr && std::strcmp(lhs, rhs) == 0;
}

}  // namespace

namespace kgema::syscall {

i64 invoke3(i64 number, i64 arg0, i64 arg1, i64 arg2) noexcept {
  g_last_number = number;
  g_last_arg0 = arg0;
  g_last_arg1 = arg1;
  g_last_arg2 = arg2;

  switch (number) {
    case kOpen:
      return Same(reinterpret_cast<const char*>(arg0), "good") ? 42 : -1;
    case kClose:
    case kSeek:
    case kChdir:
    case kMunmap:
    case kMprotect:
      return 0;
    case kRead:
      if (arg0 != 42 || arg1 == 0 || arg2 <= 0) {
        return -1;
      }
      std::memcpy(reinterpret_cast<void*>(arg1), "abcd", 4);
      return 4;
    case kWrite:
      if (arg1 == 0 || arg2 <= 0) {
        return -1;
      }
      if ((g_written_size + static_cast<std::size_t>(arg2)) > sizeof(g_written)) {
        return -1;
      }
      std::memcpy(g_written + g_written_size,
                  reinterpret_cast<const void*>(arg1),
                  static_cast<std::size_t>(arg2));
      g_written_size += static_cast<std::size_t>(arg2);
      return arg2;
    case kGetUid:
      return 5;
    case kSetUid:
      return arg0 == 5 ? 0 : -1;
    case kGetGid:
      return 6;
    case kSetGid:
      return arg0 == 6 ? 0 : -1;
    case kGetPriority:
      return 16;
    case kSetPriority:
      return arg0 == 20 ? 0 : -1;
    case kChmod:
      return Same(reinterpret_cast<const char*>(arg0), "file") &&
                 arg1 == 0600
               ? 0
               : -1;
    case kStat:
      if (arg1 == 0) {
        return -1;
      }
      *reinterpret_cast<Stat*>(arg1) = Stat{1U, 123U, 0755U, 0U, 0U, 42U};
      return 0;
    case kList:
      if (arg2 == 0) {
        return -1;
      }
      if (arg0 == 0 || arg1 != 0) {
        return 0;
      }
      *reinterpret_cast<DirEntry*>(arg2) = DirEntry{};
      std::strcpy(reinterpret_cast<DirEntry*>(arg2)->name, "entry");
      reinterpret_cast<DirEntry*>(arg2)->type = 2U;
      reinterpret_cast<DirEntry*>(arg2)->size = 9U;
      return 1;
    case kGetPid:
      return 77;
    case kSpawn:
      return Same(reinterpret_cast<const char*>(arg0), "good") ? 88 : -1;
    case kWait:
      if (arg1 != 0) {
        *reinterpret_cast<int*>(arg1) = 7;
      }
      return 0;
    case kMmap:
      return arg1 == 1 ? 0x2000 : 0x1000;
    default:
      return -1;
  }
}

i64 invoke4(i64 number, i64 arg0, i64 arg1, i64 arg2,
            i64 arg3) noexcept {
  g_last_number = number;
  g_last_arg0 = arg0;
  g_last_arg1 = arg1;
  g_last_arg2 = arg2;
  g_last_arg3 = arg3;
  if (number == kMmap && arg1 == 8) {
    return 0x3000;
  }
  return -1;
}

}  // namespace kgema::syscall

namespace {

int Fail(const char* message) {
  std::printf("kgema probe: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  const char path[] = "good";

  if (kgema::open(path) != 42 || g_last_number != kgema::syscall::kOpen) {
    return Fail("open good failed");
  }
  if (kgema::open(path, kgema::kOpenWrite) != 42 ||
      g_last_arg1 != static_cast<i64>(kgema::kOpenWrite)) {
    return Fail("open flags not forwarded");
  }
  if (kgema::open(nullptr) != -1) {
    return Fail("open null accepted");
  }

  if (!kgema::close(42)) {
    return Fail("close failed");
  }
  if (!kgema::seek(42, 5U) || g_last_arg1 != 5) {
    return Fail("seek failed");
  }

  char buffer[8]{};
  if (kgema::read(42, buffer, sizeof(buffer)) != 4 ||
      !Same(buffer, "abcd")) {
    return Fail("read failed");
  }
  if (kgema::read(42, nullptr, 1U) != -1 || kgema::read(42, buffer, 0U) != -1) {
    return Fail("read invalid args");
  }

  const char payload[] = "xyz";
  if (kgema::write(42, payload, 3U) != 3 ||
      g_written_size != 3U || !Same(g_written, "xyz")) {
    return Fail("write failed");
  }
  if (kgema::write(42, nullptr, 1U) != -1) {
    return Fail("write null accepted");
  }

  kgema::Stat stat{};
  if (!kgema::stat(path, stat) || stat.type != 1U || stat.size != 123U ||
      stat.mode != 0755U || stat.mtime != 42U) {
    return Fail("stat failed");
  }
  if (kgema::stat(nullptr, stat)) {
    return Fail("stat null accepted");
  }
  if (!kgema::chmod("file", 0600U) || kgema::chmod("bad", 0600U)) {
    return Fail("chmod");
  }
  kgema::u64 uid = 0;
  kgema::u64 gid = 0;
  if (!kgema::uid(uid) || uid != 5U || !kgema::setuid(5U) ||
      kgema::setuid(9U) || !kgema::gid(gid) || gid != 6U ||
      !kgema::setgid(6U) || kgema::setgid(9U)) {
    return Fail("identity");
  }
  kgema::u64 priority = 0;
  if (!kgema::priority(priority) || priority != 16U ||
      !kgema::setpriority(20U) || kgema::setpriority(0U)) {
    return Fail("priority");
  }

  if (!kgema::chdir(path) || g_last_number != kgema::syscall::kChdir) {
    return Fail("chdir failed");
  }

  kgema::DirEntry entry{};
  if (kgema::list(path, 0U, entry) != 1 ||
      !Same(entry.name, "entry") || entry.type != 2U || entry.size != 9U) {
    return Fail("list entry failed");
  }
  if (kgema::list(path, 1U, entry) != 0) {
    return Fail("list end failed");
  }
  if (kgema::list(nullptr, 0U, entry) != -1) {
    return Fail("list null accepted");
  }

  if (kgema::getpid() != 77) {
    return Fail("getpid failed");
  }
  if (kgema::spawn(path) != 88) {
    return Fail("spawn failed");
  }
  if (kgema::spawn(nullptr) != -1) {
    return Fail("spawn null accepted");
  }

  int status = 0;
  if (kgema::wait(88, &status) != 0 || status != 7) {
    return Fail("wait failed");
  }

  void* mapped = kgema::mmap(16U);
  if (mapped != reinterpret_cast<void*>(0x1000)) {
    return Fail("mmap failed");
  }
  if (kgema::mmap(0U) != nullptr) {
    return Fail("mmap zero accepted");
  }
  void* executable = kgema::mmap_executable(16U);
  if (executable != reinterpret_cast<void*>(0x2000)) {
    return Fail("mmap executable failed");
  }
  void* file_mapped = kgema::mmap_file(42, 0U, 16U);
  if (file_mapped != reinterpret_cast<void*>(0x3000) ||
      g_last_number != kgema::syscall::kMmap || g_last_arg1 != 8 ||
      g_last_arg2 != 42 || g_last_arg3 != 0) {
    return Fail("mmap file failed");
  }
  if (kgema::mmap_file(-1, 0U, 16U) != nullptr ||
      kgema::mmap_file(42, 0U, 0U) != nullptr) {
    return Fail("mmap file invalid args");
  }
  if (!kgema::mprotect(mapped, 16U, true, false)) {
    return Fail("mprotect failed");
  }
  if (g_last_number != kgema::syscall::kMprotect ||
      g_last_arg0 != reinterpret_cast<i64>(mapped) ||
      g_last_arg1 != 16 || g_last_arg2 != 1) {
    return Fail("mprotect args");
  }
  if (kgema::mprotect(mapped, 16U, true, true)) {
    return Fail("mprotect w+x accepted");
  }
  if (kgema::mprotect(nullptr, 16U, false, true) ||
      kgema::mprotect(mapped, 0U, false, true)) {
    return Fail("mprotect invalid args");
  }
  if (!kgema::munmap(mapped, 16U)) {
    return Fail("munmap failed");
  }
  if (kgema::munmap(nullptr, 16U) || kgema::munmap(mapped, 0U)) {
    return Fail("munmap invalid args");
  }

  // kgema::write always targets stdout.
  g_written_size = 0;
  kgema::write("hi", 2U);
  if (g_last_arg0 != 1 || g_written_size != 2U || g_written[0] != 'h' ||
      g_written[1] != 'i') {
    return Fail("io write failed");
  }

  std::printf("kgema probe: OK\n");
  return 0;
}

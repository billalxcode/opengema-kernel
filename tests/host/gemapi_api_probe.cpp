#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gemapi/error.h"
#include "gemapi/io.h"
#include "gemapi/memory.h"
#include "gemapi/process.h"
#include "kgema/file.h"
#include "kgema/io.h"
#include "kgema/memory.h"
#include "kgema/process.h"
#include "kgema/syscall.h"

namespace {

char g_output[64];
std::size_t g_output_size = 0;
const char* g_read_text = "";
bool g_exit_called = false;

bool Same(const char* lhs, const char* rhs) {
  return lhs != nullptr && rhs != nullptr && std::strcmp(lhs, rhs) == 0;
}

}  // namespace

namespace kgema {

void write(const char* data, usize size) noexcept {
  if (data == nullptr || size == 0U) {
    return;
  }
  if ((g_output_size + size) > sizeof(g_output)) {
    return;
  }
  std::memcpy(g_output + g_output_size, data, size);
  g_output_size += size;
}

i64 read(i64 descriptor, void* buffer, usize size) noexcept {
  if (descriptor != 0 || buffer == nullptr || g_read_text == nullptr) {
    return -1;
  }
  usize length = std::strlen(g_read_text);
  if (length > size) {
    length = size;
  }
  std::memcpy(buffer, g_read_text, length);
  return static_cast<i64>(length);
}

[[noreturn]] void exit(int /*status*/) noexcept {
  g_exit_called = true;
  std::abort();
}

u64 getpid() noexcept {
  return 77U;
}

i64 spawn(const char* path, const char* const* /*argv*/,
          usize /*argc*/) noexcept {
  return Same(path, "good") ? 88 : -1;
}

bool uid(u64& value) noexcept {
  value = 5U;
  return true;
}

bool setuid(u64 value) noexcept {
  return value == 5U;
}

bool gid(u64& value) noexcept {
  value = 6U;
  return true;
}

bool setgid(u64 value) noexcept {
  return value == 6U;
}

bool priority(u64& value) noexcept {
  value = 16U;
  return true;
}

bool setpriority(u64 value) noexcept {
  return value == 20U;
}

i64 wait(i64 pid, int* status) noexcept {
  if (pid == 0 || status == nullptr) {
    return -1;
  }
  *status = 7;
  return 0;
}

i64 fork() noexcept { return 99; }

i64 exec(const char* /*path*/, const char* const* /*argv*/,
         usize /*argc*/) noexcept {
  return 0;
}

void* mmap(usize size) noexcept {
  if (size == 0xDEADU) {
    return nullptr;
  }
  return reinterpret_cast<void*>(0x1000);
}

void* mmap_executable(usize size) noexcept {
  return size == 0U ? nullptr : reinterpret_cast<void*>(0x2000);
}

void* mmap_lazy(usize size) noexcept {
  return size == 0U ? nullptr : reinterpret_cast<void*>(0x3000);
}

void* mmap_shared(usize size) noexcept {
  return size == 0U ? nullptr : reinterpret_cast<void*>(0x4000);
}

void* mmap_file(i64 descriptor, u64 offset, usize size) noexcept {
  if (descriptor < 0 || size == 0U || (offset % 4096U) != 0U) {
    return nullptr;
  }
  return reinterpret_cast<void*>(0x5000);
}

bool munmap(void* address, usize size) noexcept {
  return address == reinterpret_cast<void*>(0x1000) && size == 16U;
}

}  // namespace kgema

namespace kgema::syscall {

i64 last_error() noexcept { return 0; }

}  // namespace kgema::syscall

namespace {

int Fail(const char* message) {
  std::printf("gemapi api probe: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  using gemapi::Status;

  if (gemapi::to_string(Status::ok) == nullptr ||
      !Same(gemapi::to_string(Status::not_found), "not found") ||
      !Same(gemapi::to_string(Status::io), "input/output error") ||
      !Same(gemapi::to_string(static_cast<Status>(99)), "unknown error")) {
    return Fail("to_string mapping");
  }

  const char text[] = "hello";
  gemapi::io::write(text, 5U);
  if (g_output_size != 5U || !Same(g_output, "hello")) {
    return Fail("io write");
  }
  const std::size_t before = g_output_size;
  gemapi::io::write(nullptr, 5U);
  gemapi::io::write(text, 0U);
  if (g_output_size != before) {
    return Fail("io write invalid args");
  }

  g_output_size = 0;
  std::memset(g_output, 0, sizeof(g_output));
  gemapi::io::print("abc");
  if (g_output_size != 3U || !Same(g_output, "abc")) {
    return Fail("io print");
  }
  gemapi::io::print(nullptr);
  if (g_output_size != 3U) {
    return Fail("io print null");
  }

  char line[8]{};
  g_read_text = "line\n";
  auto read = gemapi::io::read_line(line, sizeof(line));
  if (!read || read.value() != 4U || !Same(line, "line")) {
    return Fail("read_line newline");
  }

  g_read_text = "abcdefgh";
  char small[5]{};
  read = gemapi::io::read_line(small, sizeof(small));
  if (!read || read.value() != 4U || !Same(small, "abcd")) {
    return Fail("read_line capacity");
  }

  g_read_text = nullptr;
  if (gemapi::io::read_line(line, sizeof(line)).status() != Status::io) {
    return Fail("read_line io");
  }
  if (gemapi::io::read_line(nullptr, sizeof(line)).status() !=
          Status::invalid_argument ||
      gemapi::io::read_line(line, 1U).status() != Status::invalid_argument) {
    return Fail("read_line invalid args");
  }

  if (gemapi::process::id() != 77U) {
    return Fail("process id");
  }
  auto spawned = gemapi::process::spawn("good", nullptr, 0U);
  if (!spawned || spawned.value() != 88U) {
    return Fail("process spawn");
  }
  if (gemapi::process::spawn("bad", nullptr, 0U).status() != Status::not_found ||
      gemapi::process::spawn(nullptr, nullptr, 0U).status() !=
        Status::invalid_argument) {
    return Fail("process spawn errors");
  }

  const auto uid = gemapi::process::user_id();
  const auto gid = gemapi::process::group_id();
  if (!uid || uid.value() != 5U || !gid || gid.value() != 6U ||
      gemapi::process::set_user_id(5U) != Status::ok ||
      gemapi::process::set_user_id(9U) != Status::denied ||
      gemapi::process::set_group_id(6U) != Status::ok ||
      gemapi::process::set_group_id(9U) != Status::denied) {
    return Fail("identity");
  }
  const auto current_priority = gemapi::process::priority();
  if (!current_priority || current_priority.value() != 16U ||
      gemapi::process::set_priority(20U) != Status::ok ||
      gemapi::process::set_priority(0U) != Status::denied) {
    return Fail("process priority");
  }

  int wait_status = 0;
  if (gemapi::process::wait(88U, wait_status) != Status::ok ||
      wait_status != 7) {
    return Fail("process wait");
  }
  if (gemapi::process::wait(0U, wait_status) != Status::io) {
    return Fail("process wait error");
  }

  auto mapped = gemapi::memory::map(16U);
  if (!mapped || mapped.value() != reinterpret_cast<void*>(0x1000)) {
    return Fail("memory map");
  }
  if (gemapi::memory::map(0U).status() != Status::invalid_argument) {
    return Fail("memory map zero");
  }
  if (gemapi::memory::map(0xDEADU).status() != Status::no_memory) {
    return Fail("memory map failure");
  }
  const auto lazy = gemapi::memory::map_lazy(16U);
  if (!lazy || lazy.value() != reinterpret_cast<void*>(0x3000)) {
    return Fail("memory lazy map failure");
  }
  if (gemapi::memory::map_lazy(0U).status() != Status::invalid_argument) {
    return Fail("memory lazy map zero");
  }
  const auto shared = gemapi::memory::map_shared(16U);
  if (!shared || shared.value() != reinterpret_cast<void*>(0x4000)) {
    return Fail("memory shared map failure");
  }
  const auto filemap = gemapi::memory::map_file(3, 0U, 16U);
  if (!filemap || filemap.value() != reinterpret_cast<void*>(0x5000)) {
    return Fail("memory file map failure");
  }
  if (gemapi::memory::map_file(-1, 0U, 16U).status() !=
        Status::invalid_argument ||
      gemapi::memory::map_file(3, 0U, 0U).status() !=
        Status::invalid_argument ||
      gemapi::memory::map_file(3, 1U, 16U).status() != Status::no_memory) {
    return Fail("memory file map invalid args");
  }

  if (gemapi::memory::unmap(mapped.value(), 16U) != Status::ok) {
    return Fail("memory unmap");
  }
  if (gemapi::memory::unmap(nullptr, 16U) != Status::invalid_argument ||
      gemapi::memory::unmap(mapped.value(), 0U) != Status::invalid_argument) {
    return Fail("memory unmap invalid args");
  }
  if (gemapi::memory::unmap(reinterpret_cast<void*>(0x2000), 16U) !=
      Status::io) {
    return Fail("memory unmap error");
  }

  if (gemapi::from_errno(kgema::syscall::kErrNoMem) != Status::no_memory ||
      gemapi::from_errno(kgema::syscall::kErrNotFound) != Status::not_found ||
      gemapi::from_errno(kgema::syscall::kErrDenied) != Status::denied ||
      gemapi::from_errno(kgema::syscall::kErrTimeout) != Status::timeout ||
      gemapi::from_errno(-999) != Status::unknown) {
    return Fail("errno mapping");
  }

  std::printf("gemapi api probe: OK\n");
  return 0;
}

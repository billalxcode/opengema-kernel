#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/memory.h"
#include "gemapi/process.h"
#include "gemapi/sync.h"
#include "gemapi/time.h"

namespace {

struct InitMarker {
  InitMarker() {
    gemapi::io::write("init ok\n");
  }
};

InitMarker g_init_marker;

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

gemapi::usize append_text(char* out, gemapi::usize capacity,
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

gemapi::usize append_dec(char* out, gemapi::usize capacity,
                         gemapi::usize cursor, gemapi::u64 value) {
  char digits[20];
  gemapi::usize count = 0;
  do {
    digits[count] = static_cast<char>('0' + (value % 10U));
    ++count;
    value /= 10U;
  } while (value != 0U);
  while (count > 0U) {
    if (cursor >= capacity) {
      return capacity;
    }
    --count;
    out[cursor] = digits[count];
    ++cursor;
  }
  return cursor;
}

__attribute__((noinline)) int stack_recurse(unsigned depth) {
  volatile unsigned char buffer[4096];
  asm volatile("" : : "r"(buffer) : "memory");
  buffer[0] = static_cast<unsigned char>(depth);
  buffer[sizeof(buffer) - 1U] = static_cast<unsigned char>(depth + 1U);
  if (depth == 0U) {
    return buffer[0] + buffer[sizeof(buffer) - 1U];
  }
  return stack_recurse(depth - 1U) + buffer[0];
}

constexpr gemapi::u64 kFuzzIterations = 512;

// Every entry lies outside all user regions: below the 1 TiB user base,
// in kernel memory, or above the 1.5 TiB user limit. In particular, never
// use 0x10000000000 here because that is the hello text base, and unmapping
// it would kill the fuzzer itself.
constexpr gemapi::u64 kFuzzPointers[] = {
  0ULL,
  1ULL,
  0xFFFULL,
  0x1000ULL,
  0x40000000ULL,
  0x20000000000ULL,
  0x7FFFFFFFF000ULL,
  0xFFFF000000000000ULL,
  0xFFFFFFFFFFFFF000ULL,
};

constexpr gemapi::usize kFuzzSizes[] = {
  0ULL,
  1ULL,
  2ULL,
  7ULL,
  63ULL,
  64ULL,
  65ULL,
  255ULL,
  256ULL,
  257ULL,
  4095ULL,
  4096ULL,
  4097ULL,
  0x10000ULL,
  0xFFFFFFFFULL,
  0xFFFFFFFFFFFFFFFFULL,
};

constexpr gemapi::usize kFuzzMapSizes[] = {
  0ULL,
  1ULL,
  4095ULL,
  4096ULL,
  8192ULL,
  65536ULL,
  0x10000000000ULL,
  0xFFFFFFFFFFFFFFFFULL,
};

gemapi::u64 g_fuzz_state = 0x9E3779B97F4A7C15ULL;
volatile int g_fork_value = 0;

gemapi::u64 fuzz_next() noexcept {
  g_fuzz_state ^= g_fuzz_state << 13U;
  g_fuzz_state ^= g_fuzz_state >> 7U;
  g_fuzz_state ^= g_fuzz_state << 17U;
  return g_fuzz_state;
}

const char* fuzz_pointer(gemapi::u64 value) noexcept {
  constexpr gemapi::usize count =
    sizeof(kFuzzPointers) / sizeof(kFuzzPointers[0]);
  return reinterpret_cast<const char*>(kFuzzPointers[value % count]);
}

gemapi::usize fuzz_size(gemapi::u64 value) noexcept {
  constexpr gemapi::usize count = sizeof(kFuzzSizes) / sizeof(kFuzzSizes[0]);
  return kFuzzSizes[value % count];
}

gemapi::usize fuzz_map_size(gemapi::u64 value) noexcept {
  constexpr gemapi::usize count =
    sizeof(kFuzzMapSizes) / sizeof(kFuzzMapSizes[0]);
  return kFuzzMapSizes[value % count];
}

void run_fuzz() noexcept {
  const char* valid_path = "/applications/hello/hello";
  auto file = gemapi::fs::File::open(valid_path);
  char buffer[64]{};

  for (gemapi::u64 iteration = 0; iteration < kFuzzIterations; ++iteration) {
    const gemapi::u64 random = fuzz_next();
    const char* bad = fuzz_pointer(random);
    const gemapi::usize size = fuzz_size(random >> 17U);

    switch (random % 16U) {
      case 0:
        (void)gemapi::fs::File::open(bad, random);
        break;
      case 1:
        (void)gemapi::fs::stat(bad);
        break;
      case 2:
        (void)gemapi::fs::chdir(bad);
        break;
      case 3:
        (void)gemapi::fs::mkdir(bad);
        break;
      case 4:
        (void)gemapi::fs::unlink(bad);
        break;
      case 5:
        (void)gemapi::fs::rmdir(bad);
        break;
      case 6:
        (void)gemapi::fs::rename(bad, fuzz_pointer(random >> 7U));
        break;
      case 7:
        (void)gemapi::fs::chmod(bad, random);
        break;
      case 8:
        (void)gemapi::fs::chown(bad, random, random >> 32U);
        break;
      case 9:
        (void)gemapi::fs::mount(bad, fuzz_pointer(random >> 11U));
        break;
      case 10:
        (void)gemapi::fs::umount(bad);
        break;
      case 11:
        gemapi::io::write(bad, size);
        break;
      case 12:
        if (file) {
          if ((random & 0x100U) != 0U) {
            (void)file.value().read(buffer, size % 65U);
          } else {
            (void)file.value().read(const_cast<char*>(bad), size);
          }
          (void)file.value().seek(random);
        }
        break;
      case 13: {
        const gemapi::usize argc = 1U + (random % 32U);
        (void)gemapi::process::spawn(
          valid_path, reinterpret_cast<const char* const*>(bad), argc);
        break;
      }
      case 14: {
        int status = 0;
        (void)gemapi::process::wait(random % 16U, status);
        break;
      }
      case 15: {
        const gemapi::usize map_size = fuzz_map_size(random >> 29U);
        const auto mapped = gemapi::memory::map(map_size);
        if (mapped) {
          (void)gemapi::memory::unmap(mapped.value(), map_size);
        }
        (void)gemapi::memory::unmap(const_cast<char*>(bad), size);
        break;
      }
      default:
        break;
    }
  }
  gemapi::io::write("fuzz: arguments done\n");
}

}  // namespace

int main(int argc, char** argv, char** envp) {
  (void)envp;
  if (argc > 1 && same(argv[1], "wx")) {
    volatile unsigned char* code =
      static_cast<volatile unsigned char*>(__builtin_return_address(0));
    *code = 0;
    gemapi::io::write("code write allowed\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "nx")) {
    const auto mapped = gemapi::memory::map(4096U);
    if (!mapped) {
      gemapi::io::write("nx: map failed\n");
      return 1;
    }
    unsigned char* page = static_cast<unsigned char*>(mapped.value());
    page[0] = 0xC0U;
    page[1] = 0x03U;
    page[2] = 0x5FU;
    page[3] = 0xD6U;
    using Entry = void (*)();
    const Entry entry = reinterpret_cast<Entry>(page);
    entry();
    gemapi::io::write("nx: data executed\n");
    return 1;
  }
  if (argc > 1 && same(argv[1], "badptr")) {
    const auto file =
      gemapi::fs::File::open(reinterpret_cast<const char*>(1));
    if (!file) {
      gemapi::io::write("bad pointer rejected\n");
      return 0;
    }
    gemapi::io::write("bad pointer accepted\n");
    return 1;
  }
  if (argc > 1 && same(argv[1], "fuzz")) {
    run_fuzz();
    gemapi::io::write("FUZZ-DONE\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "mem")) {
    constexpr gemapi::usize kSmallMap = 8U * 1024U * 1024U;
    constexpr gemapi::usize kLargeMap = 40U * 1024U * 1024U;
    const auto small = gemapi::memory::map(kSmallMap);
    if (!small) {
      gemapi::io::write("mem: small map failed\n");
      return 1;
    }
    if (gemapi::memory::unmap(small.value(), kSmallMap) != gemapi::Status::ok) {
      gemapi::io::write("mem: unmap failed\n");
      return 1;
    }
    const auto large = gemapi::memory::map(kLargeMap);
    if (large) {
      (void)gemapi::memory::unmap(large.value(), kLargeMap);
      gemapi::io::write("mem: limit not enforced\n");
      return 1;
    }
    gemapi::io::write("mem: limit enforced\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "errno")) {
    const auto missing = gemapi::fs::File::open("/no-such-file");
    const auto bad =
      gemapi::fs::File::open(reinterpret_cast<const char*>(1));
    const auto big = gemapi::memory::map(40U * 1024U * 1024U);
    if (missing || bad) {
      gemapi::io::write("errno: unexpected open success\n");
      return 1;
    }
    if (big) {
      (void)gemapi::memory::unmap(big.value(), 40U * 1024U * 1024U);
      gemapi::io::write("errno: unexpected map success\n");
      return 1;
    }
    gemapi::io::write("errno: missing ");
    gemapi::io::print(gemapi::to_string(missing.status()));
    gemapi::io::write("\nerrno: bad ");
    gemapi::io::print(gemapi::to_string(bad.status()));
    gemapi::io::write("\nerrno: big ");
    gemapi::io::print(gemapi::to_string(big.status()));
    gemapi::io::write("\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "shm")) {
    const auto mapped = gemapi::memory::map_shared(4096U);
    if (!mapped) {
      gemapi::io::write("shm: map failed\n");
      return 1;
    }
    volatile unsigned char* page =
      static_cast<volatile unsigned char*>(mapped.value());
    page[0] = 0x11U;
    const auto child = gemapi::process::fork();
    if (!child) {
      gemapi::io::write("shm: fork failed\n");
      return 1;
    }
    if (child.value() == 0U) {
      page[0] = 0x22U;
      gemapi::io::write("shm: child\n");
      return 0;
    }
    int status = 0;
    if (gemapi::process::wait(child.value(), status) != gemapi::Status::ok) {
      gemapi::io::write("shm: wait failed\n");
      return 1;
    }
    if (page[0] != 0x22U) {
      gemapi::io::write("shm: parent mismatch\n");
      return 1;
    }
    if (gemapi::memory::unmap(mapped.value(), 4096U) != gemapi::Status::ok) {
      gemapi::io::write("shm: unmap failed\n");
      return 1;
    }
    gemapi::io::write("shm: parent\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "futex")) {
    const auto mapped = gemapi::memory::map_shared(4096U);
    if (!mapped) {
      gemapi::io::write("futex: map failed\n");
      return 1;
    }
    int* value = static_cast<int*>(mapped.value());
    int* ready = value + 1;
    *value = 0;
    *ready = 0;
    const auto child = gemapi::process::fork();
    if (!child) {
      gemapi::io::write("futex: fork failed\n");
      return 1;
    }
    if (child.value() == 0U) {
      *ready = 1;
      while (*value == 0) {
        if (gemapi::sync::wait(value, 0) != gemapi::Status::ok) {
          gemapi::io::write("futex: child wait failed\n");
          return 1;
        }
      }
      gemapi::io::write("futex: child woke\n");
      return 0;
    }

    gemapi::u64 waits = 0;
    while (*ready == 0 && waits < 1000U) {
      (void)gemapi::time::sleep_ms(1U);
      ++waits;
    }
    if (*ready == 0) {
      gemapi::io::write("futex: child not ready\n");
      return 1;
    }
    (void)gemapi::time::sleep_ms(50U);
    *value = 1;
    const auto woken = gemapi::sync::wake(value, 1U);
    if (!woken || woken.value() != 1U) {
      gemapi::io::write("futex: wake failed\n");
      return 1;
    }
    int status = 0;
    if (gemapi::process::wait(child.value(), status) != gemapi::Status::ok ||
        status != 0) {
      gemapi::io::write("futex: child failed\n");
      return 1;
    }
    if (gemapi::memory::unmap(mapped.value(), 4096U) != gemapi::Status::ok) {
      gemapi::io::write("futex: unmap failed\n");
      return 1;
    }
    gemapi::io::write("futex: parent\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "filemap")) {
    constexpr gemapi::usize kMapSize = 4096U;
    auto file = gemapi::fs::File::open("/system/config/version");
    if (!file) {
      gemapi::io::write("filemap: open failed\n");
      return 1;
    }
    const auto mapped = gemapi::memory::map_file(
      file.value().descriptor(), 0U, kMapSize);
    if (!mapped) {
      gemapi::io::write("filemap: map failed\n");
      return 1;
    }
    char* bytes = static_cast<char*>(mapped.value());
    if (bytes[0] != '0' || bytes[1] != '.' || bytes[2] != '1' ||
        bytes[3] != '\n') {
      gemapi::io::write("filemap: content mismatch\n");
      return 1;
    }
    if (bytes[4] != '\0' || bytes[kMapSize - 1U] != '\0') {
      gemapi::io::write("filemap: tail not zero\n");
      return 1;
    }
    bytes[0] = '9';
    if (gemapi::memory::unmap(mapped.value(), kMapSize) != gemapi::Status::ok) {
      gemapi::io::write("filemap: unmap failed\n");
      return 1;
    }
    auto again = gemapi::fs::File::open("/system/config/version");
    if (!again) {
      gemapi::io::write("filemap: reopen failed\n");
      return 1;
    }
    char check = '\0';
    const auto count = again.value().read(&check, 1U);
    if (!count || count.value() != 1U || check != '0') {
      gemapi::io::write("filemap: write leaked\n");
      return 1;
    }
    auto library = gemapi::fs::File::open("/system/libraries/libgemapi.so.1");
    if (!library) {
      gemapi::io::write("filemap: library open failed\n");
      return 1;
    }
    const auto info = gemapi::fs::stat("/system/libraries/libgemapi.so.1");
    if (!info || info.value().size < 4096U) {
      gemapi::io::write("filemap: library stat failed\n");
      return 1;
    }
    const gemapi::usize library_size =
      static_cast<gemapi::usize>(info.value().size);
    const auto library_map =
      gemapi::memory::map_file(library.value().descriptor(), 0U, library_size);
    if (!library_map) {
      gemapi::io::write("filemap: library map failed\n");
      return 1;
    }
    const unsigned char* header =
      static_cast<const unsigned char*>(library_map.value());
    if (header[0] != 0x7FU || header[1] != 'E' || header[2] != 'L' ||
        header[3] != 'F') {
      gemapi::io::write("filemap: library header mismatch\n");
      return 1;
    }
    if (gemapi::memory::unmap(library_map.value(), library_size) !=
        gemapi::Status::ok) {
      gemapi::io::write("filemap: library unmap failed\n");
      return 1;
    }
    gemapi::io::write("filemap: ok\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "prio")) {
    const auto initial = gemapi::process::priority();
    if (!initial || initial.value() != 16U) {
      gemapi::io::write("prio: initial failed\n");
      return 1;
    }
    if (gemapi::process::set_priority(20U) != gemapi::Status::ok) {
      gemapi::io::write("prio: lower failed\n");
      return 1;
    }
    const auto lowered = gemapi::process::priority();
    if (!lowered || lowered.value() != 20U) {
      gemapi::io::write("prio: readback failed\n");
      return 1;
    }
    if (gemapi::process::set_priority(40U) !=
        gemapi::Status::invalid_argument) {
      gemapi::io::write("prio: limit failed\n");
      return 1;
    }
    const auto user = gemapi::process::user_id();
    if (!user) {
      gemapi::io::write("prio: uid failed\n");
      return 1;
    }
    const gemapi::Status raised = gemapi::process::set_priority(0U);
    if (user.value() == 0U) {
      if (raised != gemapi::Status::ok ||
          gemapi::process::set_user_id(1U) != gemapi::Status::ok ||
          gemapi::process::set_priority(20U) != gemapi::Status::ok ||
          gemapi::process::set_priority(0U) != gemapi::Status::denied) {
        gemapi::io::write("prio: privileged path failed\n");
        return 1;
      }
    } else if (raised != gemapi::Status::denied) {
      gemapi::io::write("prio: unprivileged raise accepted\n");
      return 1;
    }
    gemapi::io::write("prio: ok\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "lazy")) {
    constexpr gemapi::usize kLazySize = 8U * 1024U * 1024U;
    const auto mapped = gemapi::memory::map_lazy(kLazySize);
    if (!mapped) {
      gemapi::io::write("lazy: map failed\n");
      return 1;
    }
    volatile unsigned char* bytes =
      static_cast<volatile unsigned char*>(mapped.value());
    bytes[0] = 0x11U;
    bytes[kLazySize - 1U] = 0x22U;
    if (bytes[0] != 0x11U || bytes[kLazySize - 1U] != 0x22U) {
      gemapi::io::write("lazy: readback failed\n");
      return 1;
    }
    if (gemapi::memory::unmap(mapped.value(), kLazySize) !=
        gemapi::Status::ok) {
      gemapi::io::write("lazy: unmap failed\n");
      return 1;
    }
    gemapi::io::write("lazy: ok\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "exec")) {
    const char* args[] = {"hello", "exec-child"};
    (void)gemapi::process::exec("/applications/hello/hello", args, 2U);
    gemapi::io::write("exec: failed\n");
    return 1;
  }
  if (argc > 1 && same(argv[1], "exec-child")) {
    gemapi::io::write("exec: child\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "fork")) {
    auto file = gemapi::fs::File::open("/system/config/version");
    if (!file) {
      gemapi::io::write("fork: open failed\n");
      return 1;
    }
    g_fork_value = 111;
    const auto child = gemapi::process::fork();
    if (!child) {
      gemapi::io::write("fork: failed\n");
      return 1;
    }
    if (child.value() == 0U) {
      g_fork_value = 222;
      if (g_fork_value != 222) {
        gemapi::io::write("fork: child cow failed\n");
        return 1;
      }
      char buffer[4];
      const auto read = file.value().read(buffer, sizeof(buffer));
      if (!read || read.value() != sizeof(buffer)) {
        gemapi::io::write("fork: child read failed\n");
        return 1;
      }
      gemapi::io::write("fork: child\n");
      return 0;
    }
    int status = 0;
    if (gemapi::process::wait(child.value(), status) != gemapi::Status::ok) {
      gemapi::io::write("fork: wait failed\n");
      return 1;
    }
    if (g_fork_value != 111) {
      gemapi::io::write("fork: parent cow failed\n");
      return 1;
    }
    char buffer[4];
    const auto read = file.value().read(buffer, sizeof(buffer));
    if (!read || read.value() != sizeof(buffer)) {
      gemapi::io::write("fork: parent read failed\n");
      return 1;
    }
    gemapi::io::write("fork: parent\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "aslr")) {
    volatile char stack_probe = 0;
    const auto mapped = gemapi::memory::map(4096U);
    if (!mapped) {
      gemapi::io::write("aslr: map failed\n");
      return 1;
    }
    char line[80];
    gemapi::usize cursor = 0;
    cursor = append_text(line, sizeof(line), cursor, "aslr: stack ");
    cursor = append_dec(line, sizeof(line), cursor,
                        reinterpret_cast<gemapi::u64>(&stack_probe));
    cursor = append_text(line, sizeof(line), cursor, " mmap ");
    cursor = append_dec(line, sizeof(line), cursor,
                        reinterpret_cast<gemapi::u64>(mapped.value()));
    cursor = append_text(line, sizeof(line), cursor, " code ");
    cursor = append_dec(line, sizeof(line), cursor,
                        reinterpret_cast<gemapi::u64>(&stack_recurse));
    cursor = append_text(line, sizeof(line), cursor, "\n");
    gemapi::io::write(line, cursor);
    (void)gemapi::memory::unmap(mapped.value(), 4096U);
    return 0;
  }
  if (argc > 1 && same(argv[1], "stack")) {
    volatile int result = stack_recurse(6U);
    if (result < 0) {
      gemapi::io::write("stack: failed\n");
      return 1;
    }
    gemapi::io::write("stack: grew\n");
    return 0;
  }
  if (argc > 1 && same(argv[1], "stackguard")) {
    (void)stack_recurse(20U);
    gemapi::io::write("stack: guard missing\n");
    return 1;
  }
  if (argc > 1 && same(argv[1], "fault")) {
    volatile int* invalid = reinterpret_cast<volatile int*>(0);
    *invalid = 1;
    return 2;
  }
  gemapi::io::write("hello from applications\n");
  return 0;
}

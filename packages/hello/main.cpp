#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/memory.h"
#include "gemapi/process.h"

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

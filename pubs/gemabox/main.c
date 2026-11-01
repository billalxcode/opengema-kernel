// gemabox: base user program for opengema-kernel.
// Freestanding C. The only assembly is the svc instruction in the syscall
// wrapper, which the ABI requires.

typedef unsigned long u64;

#define SYS_WRITE 1
#define SYS_EXIT 3
#define SYS_MMAP 5
#define SYS_MUNMAP 6
#define STDOUT 1

static long syscall3(long number, long arg0, long arg1, long arg2) {
  register long x0 __asm__("x0") = arg0;
  register long x1 __asm__("x1") = arg1;
  register long x2 __asm__("x2") = arg2;
  register long x8 __asm__("x8") = number;
  __asm__ volatile("svc #0"
                   : "+r"(x0)
                   : "r"(x1), "r"(x2), "r"(x8)
                   : "memory");
  return x0;
}

static void write_text(const char* text, u64 length) {
  (void)syscall3(SYS_WRITE, STDOUT, (long)text, (long)length);
}

static void copy_bytes(char* destination, const char* source, u64 count) {
  for (u64 index = 0; index < count; ++index) {
    destination[index] = source[index];
  }
}

__attribute__((section(".text.entry"), noreturn, used))
void _start(void) {
  static const char kHello[] = "hello from el0\n";
  static const char kMmapMessage[] = "mmap ok\n";

  write_text(kHello, sizeof(kHello) - 1U);

  const long mapped = syscall3(SYS_MMAP, 4096, 0, 0);
  if (mapped <= 0) {
    static const char kMmapFailed[] = "mmap failed\n";
    write_text(kMmapFailed, sizeof(kMmapFailed) - 1U);
    (void)syscall3(SYS_EXIT, 1, 0, 0);
    for (;;) {
    }
  }

  char* buffer = (char*)(u64)mapped;
  copy_bytes(buffer, kMmapMessage, sizeof(kMmapMessage) - 1U);
  write_text(buffer, sizeof(kMmapMessage) - 1U);

  (void)syscall3(SYS_MUNMAP, (long)buffer, 4096, 0);
  (void)syscall3(SYS_EXIT, 0, 0, 0);
  for (;;) {
  }
}

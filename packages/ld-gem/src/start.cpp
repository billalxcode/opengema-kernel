#include "kgema/process.h"

namespace ldgem {
int run(long argc, char** argv, char** envp) noexcept;
}

extern "C" {
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();
}

namespace {

void RunInitArray() {
  for (void (**entry)() = __init_array_start; entry != __init_array_end;
       ++entry) {
    (*entry)();
  }
}

}  // namespace

extern "C" {

__attribute__((noreturn, section(".text.entry"), used))
void _start(long argc, char** argv, char** envp) {
  RunInitArray();
  kgema::exit(ldgem::run(argc, argv, envp));
}

}  // extern "C"

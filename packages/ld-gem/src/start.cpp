#include "kgema/process.h"

namespace ldgem {
int run(long argc, char** argv, char** envp) noexcept;
}

extern "C" {

__attribute__((noreturn, section(".text.entry"), used))
void _start(long argc, char** argv, char** envp) {
  kgema::exit(ldgem::run(argc, argv, envp));
}

}  // extern "C"

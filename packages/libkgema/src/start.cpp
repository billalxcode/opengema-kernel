#include "kgema/process.h"

// Provided by the program. libkgema turns it into the EL0 entry point.
extern int main(int argc, char** argv, char** envp);

extern "C" {

__attribute__((noreturn, section(".text.entry"), used))
void _start(long argc, char** argv, char** envp) {
  const int status = main(static_cast<int>(argc), argv, envp);
  kgema::exit(status);
}

}  // extern "C"

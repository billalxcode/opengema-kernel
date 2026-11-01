#include "kgema/process.h"

// Provided by the program. libkgema turns it into the EL0 entry point.
extern int main();

extern "C" {

__attribute__((noreturn, section(".text.entry"), used))
void _start() {
  const int status = main();
  kgema::exit(status);
}

}  // extern "C"

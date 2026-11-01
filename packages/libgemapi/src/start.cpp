#include "gemapi/process.h"

// Provided by the program. libgemapi turns it into the EL0 entry point. This
// file is part of the static archive only; the shared object exposes the API
// without claiming the process entry point.

extern int main(int argc, char** argv, char** envp);

extern "C" {

__attribute__((noreturn, section(".text.entry"), used))
void _start(long argc, char** argv, char** envp) {
  const int status = main(static_cast<int>(argc), argv, envp);
  gemapi::process::exit(status);
}

}  // extern "C"

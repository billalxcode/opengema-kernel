#include "gemapi/process.h"

// Provided by the program. libgemapi turns it into the EL0 entry point. This
// file is part of the static archive only; the shared object exposes the API
// without claiming the process entry point.

extern int main(int argc, char** argv, char** envp);

extern "C" {
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();
}

namespace {

bool HasDynamicEntry(char** envp) {
  constexpr char kPrefix[] = "GEM_ENTRY=";
  if (envp == nullptr) {
    return false;
  }
  for (int index = 0; envp[index] != nullptr; ++index) {
    const char* value = envp[index];
    int offset = 0;
    while (kPrefix[offset] != '\0' && value[offset] == kPrefix[offset]) {
      ++offset;
    }
    if (kPrefix[offset] == '\0') {
      return true;
    }
  }
  return false;
}

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
  // ld-gem already runs the main executable initializers for dynamic images.
  if (!HasDynamicEntry(envp)) {
    RunInitArray();
  }
  const int status = main(static_cast<int>(argc), argv, envp);
  gemapi::process::exit(status);
}

}  // extern "C"

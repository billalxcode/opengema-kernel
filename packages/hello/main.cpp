#include "gemapi/io.h"

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

}  // namespace

int main(int argc, char** argv, char** envp) {
  (void)envp;
  if (argc > 1 && same(argv[1], "fault")) {
    volatile int* invalid = reinterpret_cast<volatile int*>(0);
    *invalid = 1;
    return 2;
  }
  gemapi::io::write("hello from applications\n");
  return 0;
}

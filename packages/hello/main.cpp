#include "gemapi/io.h"

namespace {

struct InitMarker {
  InitMarker() {
    gemapi::io::write("init ok\n");
  }
};

InitMarker g_init_marker;

}  // namespace

int main(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  (void)envp;
  gemapi::io::write("hello from applications\n");
  return 0;
}

#include "gemapi/io.h"

int main(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  (void)envp;
  gemapi::io::write("hello from applications\n");
  return 0;
}

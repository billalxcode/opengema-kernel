#include "gemapi/io.h"

int main(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  (void)envp;
  gemapi::io::write("hello from the opengema SDK\n");
  return 0;
}

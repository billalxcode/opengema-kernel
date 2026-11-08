#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int argc, char** argv, char** /*envp*/) {
  if (argc < 2) {
    gemapi::io::write("touch: usage: touch <path>\n");
    return 1;
  }
  auto file = gemapi::fs::File::open(
    argv[1], gemapi::fs::kOpenCreate | gemapi::fs::kOpenAppend);
  if (!file) {
    gemapi::io::write("touch: failed\n");
    return 1;
  }
  (void)file.value().close();
  gemapi::io::write("touch: ok\n");
  return 0;
}

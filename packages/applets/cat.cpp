#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int argc, char** argv, char** /*envp*/) {
  if (argc < 2) {
    gemapi::io::write("cat: usage: cat <path>...\n");
    return 1;
  }
  char buffer[256];
  for (int index = 1; index < argc; ++index) {
    auto file = gemapi::fs::File::open(argv[index]);
    if (!file) {
      gemapi::io::write("cat: open failed\n");
      return 1;
    }
    for (;;) {
      auto count = file.value().read(buffer, sizeof(buffer));
      if (!count) {
        gemapi::io::write("cat: read failed\n");
        return 1;
      }
      if (count.value() == 0U) {
        break;
      }
      gemapi::io::write(buffer, count.value());
    }
  }
  return 0;
}

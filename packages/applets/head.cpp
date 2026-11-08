#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int argc, char** argv, char** /*envp*/) {
  if (argc < 2) {
    gemapi::io::write("head: usage: head <path>\n");
    return 1;
  }
  auto file = gemapi::fs::File::open(argv[1]);
  if (!file) {
    gemapi::io::write("head: failed\n");
    return 1;
  }
  char buffer[64];
  gemapi::u64 lines = 0;
  while (lines < 10U) {
    auto count = file.value().read(buffer, sizeof(buffer));
    if (!count) {
      gemapi::io::write("head: read failed\n");
      return 1;
    }
    if (count.value() == 0U) {
      break;
    }
    for (gemapi::usize index = 0; index < count.value(); ++index) {
      const char value = buffer[index];
      (void)gemapi::io::write(&value, 1U);
      if (value == '\n') {
        ++lines;
        if (lines >= 10U) {
          break;
        }
      }
    }
  }
  return 0;
}

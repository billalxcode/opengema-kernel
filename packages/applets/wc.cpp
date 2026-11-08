#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int argc, char** argv, char** /*envp*/) {
  if (argc < 2) {
    gemapi::io::write("wc: usage: wc <path>\n");
    return 1;
  }
  auto file = gemapi::fs::File::open(argv[1]);
  if (!file) {
    gemapi::io::write("wc: failed\n");
    return 1;
  }
  char buffer[64];
  gemapi::u64 bytes = 0;
  gemapi::u64 lines = 0;
  for (;;) {
    auto count = file.value().read(buffer, sizeof(buffer));
    if (!count) {
      gemapi::io::write("wc: read failed\n");
      return 1;
    }
    if (count.value() == 0U) {
      break;
    }
    for (gemapi::usize index = 0; index < count.value(); ++index) {
      ++bytes;
      if (buffer[index] == '\n') {
        ++lines;
      }
    }
  }
  gemapi::io::write("lines: ");
  char digits[20];
  gemapi::usize length = 0;
  gemapi::u64 value = lines;
  do {
    digits[length] = static_cast<char>('0' + (value % 10U));
    ++length;
    value /= 10U;
  } while (value != 0U);
  while (length > 0U) {
    --length;
    (void)gemapi::io::write(&digits[length], 1U);
  }
  gemapi::io::write(" bytes: ");
  length = 0;
  value = bytes;
  do {
    digits[length] = static_cast<char>('0' + (value % 10U));
    ++length;
    value /= 10U;
  } while (value != 0U);
  while (length > 0U) {
    --length;
    (void)gemapi::io::write(&digits[length], 1U);
  }
  gemapi::io::write("\n");
  return 0;
}

#include "gemapi/fs.h"
#include "gemapi/io.h"

namespace {

bool Contains(const char* text, const char* pattern) {
  if (pattern[0] == '\0') {
    return true;
  }
  for (gemapi::usize start = 0; text[start] != '\0'; ++start) {
    gemapi::usize index = 0;
    while (pattern[index] != '\0' && text[start + index] != '\0' &&
           text[start + index] == pattern[index]) {
      ++index;
    }
    if (pattern[index] == '\0') {
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char** argv, char** /*envp*/) {
  if (argc < 3) {
    gemapi::io::write("grep: usage: grep <pattern> <path>\n");
    return 1;
  }
  auto file = gemapi::fs::File::open(argv[2]);
  if (!file) {
    gemapi::io::write("grep: failed\n");
    return 1;
  }
  char line[128];
  gemapi::usize line_length = 0;
  char buffer[64];
  for (;;) {
    auto count = file.value().read(buffer, sizeof(buffer));
    if (!count) {
      gemapi::io::write("grep: read failed\n");
      return 1;
    }
    if (count.value() == 0U && line_length == 0U) {
      break;
    }
    for (gemapi::usize index = 0; index < count.value(); ++index) {
      const char value = buffer[index];
      if (value == '\n') {
        line[line_length] = '\0';
        if (Contains(line, argv[1])) {
          gemapi::io::write(line, line_length);
          gemapi::io::write("\n");
        }
        line_length = 0;
        continue;
      }
      if (line_length < (sizeof(line) - 1U)) {
        line[line_length] = value;
        ++line_length;
      }
    }
    if (count.value() == 0U) {
      line[line_length] = '\0';
      if (line_length > 0U && Contains(line, argv[1])) {
        gemapi::io::write(line, line_length);
        gemapi::io::write("\n");
      }
      break;
    }
  }
  return 0;
}

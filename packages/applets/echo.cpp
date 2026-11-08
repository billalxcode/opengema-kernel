#include "gemapi/io.h"

namespace {

void WriteText(const char* text) {
  gemapi::usize length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  gemapi::io::write(text, length);
}

}  // namespace

int main(int argc, char** argv, char** /*envp*/) {
  for (int index = 1; index < argc; ++index) {
    if (index > 1) {
      gemapi::io::write(" ");
    }
    WriteText(argv[index]);
  }
  gemapi::io::write("\n");
  return 0;
}

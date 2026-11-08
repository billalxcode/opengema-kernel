#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int /*argc*/, char** /*argv*/, char** /*envp*/) {
  auto file = gemapi::fs::File::open("/proc/meminfo");
  if (!file) {
    gemapi::io::write("free: open failed\n");
    return 1;
  }
  char buffer[256];
  for (;;) {
    auto count = file.value().read(buffer, sizeof(buffer));
    if (!count) {
      gemapi::io::write("free: read failed\n");
      return 1;
    }
    if (count.value() == 0U) {
      return 0;
    }
    gemapi::io::write(buffer, count.value());
  }
}

#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int /*argc*/, char** /*argv*/, char** /*envp*/) {
  char path[128];
  const auto current = gemapi::fs::cwd(path, sizeof(path));
  if (!current) {
    gemapi::io::write("pwd: failed\n");
    return 1;
  }
  gemapi::io::write("pwd: ");
  gemapi::io::write(path);
  gemapi::io::write("\n");
  return 0;
}

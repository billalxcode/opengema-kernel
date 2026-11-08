#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int argc, char** argv, char** /*envp*/) {
  const char* path = argc > 1 ? argv[1] : "/";
  for (gemapi::u64 index = 0;; ++index) {
    gemapi::fs::DirEntry entry{};
    auto found = gemapi::fs::list(path, index, entry);
    if (!found) {
      gemapi::io::write("ls: list failed\n");
      return 1;
    }
    if (!found.value()) {
      return 0;
    }
    gemapi::io::write(entry.name);
    if (entry.kind == gemapi::fs::Kind::directory) {
      gemapi::io::write("/");
    }
    gemapi::io::write("\n");
  }
}

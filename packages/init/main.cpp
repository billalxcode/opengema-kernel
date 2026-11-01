#include "gemapi/error.h"
#include "gemapi/io.h"
#include "gemapi/paths.h"
#include "gemapi/process.h"
#include "gemapi/types.h"

namespace {

constexpr char kShellName[] = "gemabox";
constexpr char kShellArgument[] = "shell";

}  // namespace

int main(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  (void)envp;

  constexpr int kMaxRestarts = 2;
  for (int attempt = 0; attempt <= kMaxRestarts; ++attempt) {
    const char* shell_argv[] = {kShellName, kShellArgument};
    auto shell =
      gemapi::process::spawn(gemapi::paths::kGemabox, shell_argv, 2);
    if (!shell) {
      gemapi::io::write("init: cannot start shell\n");
      return 1;
    }

    int status = 0;
    if (gemapi::process::wait(shell.value(), status) != gemapi::Status::ok) {
      gemapi::io::write("init: wait failed\n");
      return 1;
    }
    if (status == 0) {
      return 0;
    }
    gemapi::io::write("init: shell failed, restarting\n");
  }
  return 1;
}

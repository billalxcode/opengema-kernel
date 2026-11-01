#include "gemabox/applets.h"

#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/paths.h"

namespace {

constexpr char kWriteMessage[] = "write ok\n";

}  // namespace

namespace gemabox {

int run_writer() {
  if (gemapi::fs::chdir(gemapi::paths::kTmp) != gemapi::Status::ok) {
    return 1;
  }
  auto file = gemapi::fs::File::open("writer", gemapi::fs::kOpenWrite);
  if (!file) {
    return 1;
  }
  const gemapi::usize message_size = sizeof(kWriteMessage) - 1U;
  if (file.value().write(kWriteMessage, message_size) != gemapi::Status::ok) {
    return 1;
  }
  if (file.value().seek(0) != gemapi::Status::ok) {
    return 1;
  }
  char readback[16];
  auto count = file.value().read(readback, sizeof(readback));
  if (!count || count.value() < message_size || readback[0] != 'w') {
    return 1;
  }
  gemapi::io::write(kWriteMessage);
  return 0;
}

}  // namespace gemabox

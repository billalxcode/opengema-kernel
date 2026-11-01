#include "gemapi/system.h"

#include "kgema/system.h"

namespace gemapi::system {

Status shutdown() noexcept {
  return kgema::shutdown() < 0 ? Status::not_supported : Status::ok;
}

Status reboot() noexcept {
  return kgema::reboot() < 0 ? Status::not_supported : Status::ok;
}

}  // namespace gemapi::system

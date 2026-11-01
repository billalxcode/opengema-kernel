#include "gemapi/error.h"

namespace gemapi {

const char* to_string(Status status) noexcept {
  switch (status) {
    case Status::ok:
      return "ok";
    case Status::invalid_argument:
      return "invalid argument";
    case Status::not_found:
      return "not found";
    case Status::denied:
      return "permission denied";
    case Status::busy:
      return "busy";
    case Status::io:
      return "input/output error";
    case Status::no_memory:
      return "out of memory";
    case Status::not_supported:
      return "not supported";
    case Status::unknown:
      break;
  }
  return "unknown error";
}

}  // namespace gemapi

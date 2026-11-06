#include "gemapi/error.h"

#include "kgema/syscall.h"

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
    case Status::no_device:
      return "no device";
    case Status::exists:
      return "already exists";
    case Status::timeout:
      return "timeout";
    case Status::not_implemented:
      return "not implemented";
    case Status::not_initialized:
      return "not initialized";
    case Status::unknown:
      break;
  }
  return "unknown error";
}

Status from_errno(i64 value) noexcept {
  switch (value) {
    case kgema::syscall::kErrPermission:
    case kgema::syscall::kErrDenied:
      return Status::denied;
    case kgema::syscall::kErrNotFound:
      return Status::not_found;
    case kgema::syscall::kErrIo:
      return Status::io;
    case kgema::syscall::kErrNoMem:
      return Status::no_memory;
    case kgema::syscall::kErrBusy:
      return Status::busy;
    case kgema::syscall::kErrExists:
      return Status::exists;
    case kgema::syscall::kErrNoDevice:
      return Status::no_device;
    case kgema::syscall::kErrInvalid:
      return Status::invalid_argument;
    case kgema::syscall::kErrNoSys:
      return Status::not_implemented;
    case kgema::syscall::kErrUnsupported:
      return Status::not_supported;
    case kgema::syscall::kErrTimeout:
      return Status::timeout;
    default:
      return Status::unknown;
  }
}

}  // namespace gemapi

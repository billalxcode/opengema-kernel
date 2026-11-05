#include "og/syscall/syscall_internal.h"

#include "og/core/log.h"

namespace og::syscall {

Status DoGetLogLevel(uint64_t& result) noexcept {
  result = static_cast<uint64_t>(GetLogLevel());
  return Status::kOk;
}

Status DoSetLogLevel(uint64_t level, uint64_t& result) noexcept {
  if (level > static_cast<uint64_t>(LogLevel::kDebug)) {
    return Status::kInvalidArg;
  }
  SetLogLevel(static_cast<LogLevel>(level));
  result = level;
  return Status::kOk;
}

}  // namespace og::syscall

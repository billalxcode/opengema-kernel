#include "og/syscall/syscall_internal.h"

#include <span>

#include "og/core/log.h"
#include "og/drivers/uart.h"
#include "og/fs/vfs.h"
#include "og/io/print.h"
#include "og/io/tty.h"
#include "og/mem/vmm.h"
#include "og/sched/sched.h"

namespace og::syscall::detail {

Status TtyReadLine(const mem::AddressSpace& space, uint64_t user_buffer,
                   uint64_t length, uint64_t& result) noexcept {
  if (length < 2U) {
    return Status::kInvalidArg;
  }
  const uint64_t capacity = length > kTtyLine ? kTtyLine : length;
  char line[kTtyLine];
  uint64_t count = 0;

  for (;;) {
    char value = '\0';
    if (!io::tty::PopChar(value)) {
      drivers::uart::Pl011* uart = drivers::uart::Active();
      if (uart == nullptr) {
        return Status::kNotInit;
      }
      const uint32_t received = uart->read(std::span<char>(&value, 1));
      if (received == 0U) {
        (void)sched::ThreadSleep(1);
        continue;
      }
    }
    if (value == '\r' || value == '\n') {
      (void)PrintText("\n");
      break;
    }
    if (value == '\b' || static_cast<uint8_t>(value) == 0x7FU) {
      if (count > 0U) {
        --count;
        (void)Print(std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>("\b \b"), 3));
      }
      continue;
    }
    if (value < 0x20 || value > 0x7E) {
      continue;
    }
    if ((count + 1U) < capacity) {
      line[count] = value;
      ++count;
      (void)Print(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(&value), 1));
    }
  }

  const Status status = mem::VmmCopyOut(space, user_buffer, line, count);
  if (IsErr(status)) {
    return status;
  }
  result = count;
  return Status::kOk;
}

}  // namespace og::syscall::detail

namespace og::syscall {

using namespace detail;

Status DoRead(uint64_t descriptor, uint64_t user_buffer, uint64_t length,
              uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (length == 0U) {
    result = 0;
    return Status::kOk;
  }
  if (descriptor > 0xFFFFFFFFULL) {
    return Status::kInvalidArg;
  }
  if (descriptor == 0U) {
    return TtyReadLine(process->space, user_buffer, length, result);
  }

  const uint64_t chunk = length > kMaxReadChunk ? kMaxReadChunk : length;
  uint8_t buffer[kMaxReadChunk];
  uint64_t read = 0;
  Status status = fs::FsRead(process->files, static_cast<uint32_t>(descriptor),
                             std::span<uint8_t>(buffer, chunk), read);
  if (IsErr(status)) {
    return status;
  }
  if (read > 0U) {
    status = mem::VmmCopyOut(process->space, user_buffer, buffer, read);
    if (IsErr(status)) {
      return status;
    }
  }
  result = read;
  return Status::kOk;
}

Status DoWrite(uint64_t descriptor, uint64_t user_buffer, uint64_t length,
               uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (length == 0U) {
    result = 0;
    return Status::kOk;
  }
  if (descriptor > 0xFFFFFFFFULL) {
    return Status::kInvalidArg;
  }

  char text[kMaxWriteChunk + 1U];
  uint64_t total = 0;
  while (total < length) {
    const uint64_t remaining = length - total;
    const uint64_t chunk =
      remaining > kMaxWriteChunk ? kMaxWriteChunk : remaining;
    const Status copy =
      mem::VmmCopyIn(process->space, user_buffer + total, text, chunk);
    if (IsErr(copy)) {
      LogHex(LogLevel::kError, "syscall: copy failed",
             static_cast<uint64_t>(copy));
      return copy;
    }

    if (descriptor == kStdout) {
      text[chunk] = '\0';
      const Status status = PrintText(text);
      if (IsErr(status)) {
        LogHex(LogLevel::kError, "syscall: write failed",
               static_cast<uint64_t>(status));
        return status;
      }
      total += chunk;
      continue;
    }

    uint64_t written = 0;
    const Status status = fs::FsWrite(
      process->files, static_cast<uint32_t>(descriptor),
      std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text), chunk),
      written);
    if (IsErr(status)) {
      return status;
    }
    if (written == 0U) {
      return Status::kIoError;
    }
    total += written;
  }

  result = total;
  return Status::kOk;
}

}  // namespace og::syscall

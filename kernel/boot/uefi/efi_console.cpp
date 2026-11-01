#include "og/boot/uefi/efi_console.h"

#include <cstddef>

namespace og::boot::uefi {

namespace {

constexpr std::size_t kChunk = 64;

Status FirmwareWrite(void* context, std::span<const uint8_t> bytes) noexcept {
  if (context == nullptr) {
    return Status::kNotInit;
  }
  if (bytes.empty()) {
    return Status::kInvalidArg;
  }
  auto* output = static_cast<EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*>(context);

  CHAR16 line[kChunk];
  std::size_t index = 0;
  for (uint8_t byte : bytes) {
    line[index] = static_cast<CHAR16>(byte);
    ++index;
    if (index == (kChunk - 1)) {
      line[index] = 0;
      const EFI_STATUS status = output->output_string(output, line);
      if (EfiFailed(status)) {
        return Status::kIoError;
      }
      index = 0;
    }
  }
  if (index > 0) {
    line[index] = 0;
    const EFI_STATUS status = output->output_string(output, line);
    if (EfiFailed(status)) {
      return Status::kIoError;
    }
  }
  return Status::kOk;
}

}  // namespace

io::Console MakeFirmwareConsole(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* output) noexcept {
  if (output == nullptr) {
    return io::Console{};
  }
  if (output->output_string == nullptr) {
    return io::Console{};
  }
  return io::Console(&FirmwareWrite, output);
}

}  // namespace og::boot::uefi

#pragma once

// UEFI firmware console adapter. Only valid before ExitBootServices.

#include "efi.h"
#include "og/console.h"

namespace og::boot::uefi {

[[nodiscard]] io::Console MakeFirmwareConsole(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* output) noexcept;

}  // namespace og::boot::uefi

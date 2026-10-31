#include "efi.h"
#include "efi_boot.h"

#include "og/bootinfo.h"
#include "og/kmain.h"
#include "og/log.h"
#include "og/memory_map.h"
#include "og/panic.h"
#include "og/status.h"
#include "og/uart.h"

namespace {

og::BootInfo g_boot_info{};

}  // namespace

extern "C" EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE* system_table) {
  if (system_table == nullptr) {
    return 1;
  }

  og::drivers::uart::Pl011 uart;
  const og::Status uart_status = uart.init(og::memmap::kUart0Base);
  if (og::IsErr(uart_status)) {
    return 1;
  }
  og::drivers::uart::RegisterAsLogSink(uart);
  og::Log(og::LogLevel::kInfo, "opengema-kernel: uefi entry\n");

  // Firmware console is only valid before ExitBootServices.
  if (system_table->con_out == nullptr) {
    og::Log(og::LogLevel::kWarn, "uefi: no ConOut, uart only\n");
  } else {
    (void)system_table->con_out->output_string(
      system_table->con_out, L"opengema-kernel: UEFI console ready\r\n");
  }

  g_boot_info.uart_base = uart.base();

  const og::Status takeover = og::boot::uefi::TakeOver(image, system_table, &g_boot_info);
  if (og::IsErr(takeover)) {
    og::Panic("uefi: ExitBootServices failed");
  }
  og::Log(og::LogLevel::kInfo, "uefi: boot services exited\n");

  og::kmain(&g_boot_info);
  og::Panic("efi_main: kmain returned");
}

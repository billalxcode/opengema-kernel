#include "efi.h"
#include "efi_boot.h"
#include "efi_console.h"

#include "og/bootinfo.h"
#include "og/console.h"
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
  const og::Status uart_status = uart.init(og::memmap::kUart0Base, og::memmap::kUart0Size);
  if (og::IsErr(uart_status)) {
    return 1;
  }
  og::io::SetConsole(og::drivers::uart::MakeConsole(uart));
  og::Log(og::LogLevel::kInfo, "opengema-kernel: uefi entry\n");

  // The firmware console dies with ExitBootServices, so use it only here.
  if (system_table->con_out == nullptr) {
    og::Log(og::LogLevel::kWarn, "uefi: no ConOut\n");
  } else {
    const og::io::Console firmware_console =
      og::boot::uefi::MakeFirmwareConsole(system_table->con_out);
    (void)og::io::ConsoleWriteText(firmware_console, "opengema-kernel: UEFI console ready\n");
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

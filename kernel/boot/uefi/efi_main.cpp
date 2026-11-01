#include "efi.h"
#include "efi_boot.h"
#include "efi_console.h"

#include "og/core/bootinfo.h"
#include "og/core/kmain.h"
#include "og/core/log.h"
#include "og/core/panic.h"
#include "og/core/status.h"
#include "og/drivers/uart.h"
#include "og/io/console.h"
#include "og/platform/memory_map.h"

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
  og::drivers::uart::SetActive(&uart);
  og::Log(og::LogLevel::kInfo, "opengema-kernel: uefi entry\n");

  // QEMU with acpi=off passes the device tree through a config table.
  for (UINTN index = 0; index < system_table->number_of_table_entries; ++index) {
    const auto* table =
      static_cast<const EFI_CONFIGURATION_TABLE*>(system_table->configuration_table) + index;
    const EFI_GUID& guid = table->vendor_guid;
    if (guid.data1 != 0xB1B621D5U || guid.data2 != 0xF19CU || guid.data3 != 0x41A5U) {
      continue;
    }
    if (table->vendor_table == nullptr) {
      continue;
    }
    const auto* header = static_cast<const uint8_t*>(table->vendor_table);
    const uint32_t total = (static_cast<uint32_t>(header[4]) << 24U) |
                           (static_cast<uint32_t>(header[5]) << 16U) |
                           (static_cast<uint32_t>(header[6]) << 8U) |
                           static_cast<uint32_t>(header[7]);
    g_boot_info.dtb_address = reinterpret_cast<uint64_t>(table->vendor_table);
    g_boot_info.dtb_size = total;
    break;
  }

  // The firmware console dies with ExitBootServices, so use it only here.
  if (system_table->con_out == nullptr) {
    og::Log(og::LogLevel::kWarn, "uefi: no ConOut\n");
  } else {
    const og::io::Console firmware_console =
      og::boot::uefi::MakeFirmwareConsole(system_table->con_out);
    (void)og::io::ConsoleWriteText(firmware_console, "opengema-kernel: UEFI console ready\n");
  }

  g_boot_info.uart_base = uart.base();

  const og::Status image_status =
    og::boot::uefi::QueryImageSections(image, system_table, &g_boot_info);
  if (og::IsErr(image_status)) {
    og::Log(og::LogLevel::kWarn, "uefi: image sections unavailable\n");
  }

  const og::Status takeover = og::boot::uefi::TakeOver(image, system_table, &g_boot_info);
  if (og::IsErr(takeover)) {
    og::Panic("uefi: ExitBootServices failed");
  }
  og::Log(og::LogLevel::kInfo, "uefi: boot services exited\n");

  og::kmain(&g_boot_info);
  og::Panic("efi_main: kmain returned");
}

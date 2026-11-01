#include "efi.h"
#include "og/boot/uefi/efi_boot.h"
#include "og/boot/uefi/efi_console.h"

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

constexpr EFI_GUID kDtbGuid{
  0xB1B621D5U, 0xF19CU, 0x41A5U,
  {0x83, 0x0B, 0xD9, 0x15, 0x2C, 0x69, 0xAA, 0xE0}};
constexpr EFI_GUID kAcpi20Guid{
  0x8868E871U, 0xE4F1U, 0x11D3U,
  {0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81}};
constexpr EFI_GUID kAcpi10Guid{
  0xEB9D2D30U, 0x2D88U, 0x11D3U,
  {0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}};

[[nodiscard]] bool GuidMatches(const EFI_GUID& value,
                               const EFI_GUID& expected) noexcept {
  return value.data1 == expected.data1 && value.data2 == expected.data2 &&
         value.data3 == expected.data3 && value.data4[0] == expected.data4[0] &&
         value.data4[1] == expected.data4[1] &&
         value.data4[2] == expected.data4[2] &&
         value.data4[3] == expected.data4[3] &&
         value.data4[4] == expected.data4[4] &&
         value.data4[5] == expected.data4[5] &&
         value.data4[6] == expected.data4[6] &&
         value.data4[7] == expected.data4[7];
}

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
  og::io::SetLogConsole(og::io::ActiveConsole());
  og::drivers::uart::SetActive(&uart);
  og::Log(og::LogLevel::kInfo, "opengema-kernel: uefi entry\n");

  // QEMU passes either a device tree or ACPI tables through the UEFI
  // configuration table, depending on the machine acpi property.
  for (UINTN index = 0; index < system_table->number_of_table_entries; ++index) {
    const auto* table =
      static_cast<const EFI_CONFIGURATION_TABLE*>(system_table->configuration_table) + index;
    if (table->vendor_table == nullptr) {
      continue;
    }
    if (GuidMatches(table->vendor_guid, kDtbGuid) &&
        g_boot_info.dtb_address == 0) {
      const auto* header = static_cast<const uint8_t*>(table->vendor_table);
      const uint32_t total = (static_cast<uint32_t>(header[4]) << 24U) |
                             (static_cast<uint32_t>(header[5]) << 16U) |
                             (static_cast<uint32_t>(header[6]) << 8U) |
                             static_cast<uint32_t>(header[7]);
      g_boot_info.dtb_address = reinterpret_cast<uint64_t>(table->vendor_table);
      g_boot_info.dtb_size = total;
      continue;
    }
    if ((GuidMatches(table->vendor_guid, kAcpi20Guid) ||
         GuidMatches(table->vendor_guid, kAcpi10Guid)) &&
        g_boot_info.acpi_rsdp == 0) {
      g_boot_info.acpi_rsdp =
        reinterpret_cast<uint64_t>(table->vendor_table);
    }
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

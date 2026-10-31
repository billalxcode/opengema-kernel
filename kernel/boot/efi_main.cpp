#include "efi.h"

#include "og/memory_map.h"
#include "og/status.h"
#include "og/uart.h"

namespace og {
void kmain() noexcept;
}

namespace {

void efi_print(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* out, const CHAR16* s) {
  if (out == nullptr) {
    return;
  }
  if (s == nullptr) {
    return;
  }
  out->output_string(out, s);
}

}  // namespace

extern "C" EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE* st) {
  (void)image;
  if (st == nullptr) {
    return 1;
  }

  og::arch::aarch64::UartPl011 uart;
  if (!og::IsOk(uart.init(og::memmap::kUart0Base))) {
    return 1;
  }
  uart.puts("opengema-kernel: EFI boot\n");

  if (st->con_out == nullptr) {
    uart.puts("warn: no ConOut, use UART only\n");
  } else {
    efi_print(st->con_out, L"opengema-kernel: hello UEFI\r\n");
  }

  uart.puts("hello UART PL011\n");
  og::kmain();
  uart.puts("efi_main: done, halt\n");

  while (true) {
    __asm__ volatile("wfe");
  }
}

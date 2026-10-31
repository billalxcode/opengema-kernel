#include "og/memory_map.h"
#include "og/mmu.h"
#include "og/status.h"
#include "og/uart.h"

namespace og {

void kmain() noexcept {
  arch::aarch64::UartPl011 uart;
  Status s = uart.init(memmap::kUart0Base);
  if (!IsOk(s)) {
    return;
  }
  uart.puts("opengema-kernel: kmain boot\n");
  uart.puts("uart: PL011 0x09000000 ok\n");

  arch::aarch64::Mmu mmu;
  Status ms = mmu.init_identity();
  if (!IsOk(ms)) {
    uart.puts("mmu: init_identity not ready (expected for setup)\n");
    return;
  }
  uart.puts("mmu: identity map on\n");
}

}  // namespace og

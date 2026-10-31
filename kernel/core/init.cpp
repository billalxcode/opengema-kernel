#include "og/bootinfo.h"
#include "og/log.h"
#include "og/mmu.h"
#include "og/panic.h"
#include "og/status.h"

#include "og/arch/registers.h"

namespace og {

// Single entry point after the firmware hands over control. Modules are
// initialized in dependency order and the first failure stops the chain.
void kmain(BootInfo* info) noexcept {
  if (info == nullptr) {
    Panic("kmain: null boot info");
  }
  if (info->magic != kBootInfoMagic) {
    Panic("kmain: bad boot info magic");
  }
  if (info->version != kBootInfoVersion) {
    Panic("kmain: unsupported boot info version");
  }

  Log(LogLevel::kInfo, "kernel: kmain entered\n");
  LogDec(LogLevel::kInfo, "memory ranges", info->memory_count);
  LogHex(LogLevel::kInfo, "uart base", info->uart_base);

  arch::aarch64::Mmu mmu;
  const Status mmu_status = mmu.init_identity();
  if (IsErr(mmu_status)) {
    Log(LogLevel::kWarn, "mmu: identity map not ready\n");
  }

  Log(LogLevel::kInfo, "kernel: halt\n");
  arch::aarch64::HaltForever();
}

}  // namespace og

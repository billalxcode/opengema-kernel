#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/arch/trap.h"
#include "og/bootinfo.h"
#include "og/log.h"
#include "og/mmu.h"
#include "og/panic.h"
#include "og/print.h"
#include "og/status.h"

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

  // Install vectors first so any fault after this point panics with context.
  arch::aarch64::InstallVectorTable();

  Log(LogLevel::kInfo, "kernel: kmain entered\n");
  LogDec(LogLevel::kInfo, "current el", arch::aarch64::CurrentEl());
  LogDec(LogLevel::kInfo, "cpu id", arch::aarch64::CpuId());
  LogDec(LogLevel::kInfo, "memory ranges", info->memory_count);
  (void)PrintHex("uart base", info->uart_base);

  arch::aarch64::Mmu mmu;
  const Status mmu_status = mmu.init_identity(info->ram_base, info->ram_size);
  if (IsErr(mmu_status)) {
    Log(LogLevel::kError, "mmu: identity map failed\n");
  } else {
    Log(LogLevel::kInfo, "mmu: identity map active\n");
  }

  (void)PrintLn("kernel: halt");
  arch::aarch64::HaltForever();
}

}  // namespace og

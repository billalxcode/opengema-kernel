#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/arch/trap.h"
#include "og/bootinfo.h"
#include "og/heap.h"
#include "og/log.h"
#include "og/mem_selftest.h"
#include "og/panic.h"
#include "og/pmm.h"
#include "og/print.h"
#include "og/status.h"
#include "og/vmm.h"

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
  (void)PrintHex("uart base", info->uart_base);

  Status status = mem::PmmInit(*info);
  if (IsErr(status)) {
    Panic("mem: pmm init failed");
  }
  LogDec(LogLevel::kInfo, "pmm: total frames", mem::PmmTotalFrames());
  LogDec(LogLevel::kInfo, "pmm: free frames", mem::PmmFreeFrames());

  status = mem::VmmInit(*info);
  if (IsErr(status)) {
    Panic("mem: vmm init failed");
  }
  Log(LogLevel::kInfo, "vmm: identity map active\n");

  status = mem::HeapInit();
  if (IsErr(status)) {
    Panic("mem: heap init failed");
  }

  status = mem::RunSelfTest();
  if (IsErr(status)) {
    Panic("mem: self test failed");
  }
  Log(LogLevel::kInfo, "mem: self test passed\n");

  (void)PrintLn("kernel: halt");
  arch::aarch64::HaltForever();
}

}  // namespace og

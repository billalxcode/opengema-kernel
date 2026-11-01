#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/bootinfo.h"
#include "og/event_selftest.h"
#include "og/gic.h"
#include "og/heap.h"
#include "og/irq.h"
#include "og/log.h"
#include "og/memory_map.h"
#include "og/mem_selftest.h"
#include "og/panic.h"
#include "og/pmm.h"
#include "og/print.h"
#include "og/status.h"
#include "og/timer.h"
#include "og/trap.h"
#include "og/vmm.h"
#include "og/workqueue.h"

namespace og {

namespace {

constexpr uint32_t kTickHz = 100;
constexpr uint64_t kTargetTicks = 100;
uint64_t g_work_runs = 0;

void CountWork(void* /*context*/) noexcept {
  ++g_work_runs;
}

}  // namespace

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
  events::InstallVectorTable();

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

  status = events::WorkQueueInit();
  if (IsErr(status)) {
    Panic("events: workqueue init failed");
  }
  status = events::RunEventSelfTest();
  if (IsErr(status)) {
    Panic("events: self test failed");
  }

  drivers::irq::Gicv3 gic;
  status = gic.init(memmap::kGicDistBase, memmap::kGicRedistBase);
  if (IsErr(status)) {
    Panic("gic: init failed");
  }
  events::SetInterruptController(drivers::irq::MakeController(gic));
  Log(LogLevel::kInfo, "gic: ready\n");

  status = time::TimerInit();
  if (IsErr(status)) {
    Panic("timer: init failed");
  }
  status = gic.enable_irq(time::kTimerIrq);
  if (IsErr(status)) {
    Panic("timer: irq enable failed");
  }
  LogDec(LogLevel::kInfo, "timer: frequency", time::TimerFrequency());

  status = time::TimerStartPeriodic(kTickHz);
  if (IsErr(status)) {
    Panic("timer: start failed");
  }

  (void)events::ScheduleWork(&CountWork, nullptr);
  arch::aarch64::EnableIrq();

  // Bounded wait so a missing interrupt reports instead of hanging.
  const uint64_t start = arch::aarch64::ReadCntpctEl0();
  const uint64_t timeout = arch::aarch64::ReadCntfrqEl0() * 2ULL;
  while (time::TimerTicks() < kTargetTicks) {
    if ((arch::aarch64::ReadCntpctEl0() - start) > timeout) {
      break;
    }
  }
  arch::aarch64::DisableIrq();

  const uint64_t ticks = time::TimerTicks();
  LogDec(LogLevel::kInfo, "timer: ticks", ticks);
  LogDec(LogLevel::kInfo, "timer: work runs", g_work_runs);
  if (ticks < kTargetTicks) {
    Log(LogLevel::kError, "timer: no interrupts received\n");
    Panic("timer: irq not working");
  }
  Log(LogLevel::kInfo, "timer: periodic tick ok\n");

  (void)PrintLn("kernel: halt");
  arch::aarch64::HaltForever();
}

}  // namespace og

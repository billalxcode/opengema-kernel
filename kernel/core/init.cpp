#include "og/boot/init.h"

#include "memory.h"
#include "og/acpi/acpi.h"
#include "og/arch/cpu.h"
#include "og/arch/mmu.h"
#include "og/arch/registers.h"
#include "og/arch/tlb.h"
#include "og/core/bootinfo.h"
#include "og/core/log.h"
#include "og/core/log_ring.h"
#include "og/core/panic.h"
#include "og/core/power.h"
#include "og/core/power.h"
#include "og/core/status.h"
#include "og/dma/dma.h"
#include "og/dma/dma_selftest.h"
#include "og/drivers/gic.h"
#include "og/drivers/uart.h"
#include "og/events/event_selftest.h"
#include "og/events/irq.h"
#include "og/events/trap.h"
#include "og/events/workqueue.h"
#include "og/io/print.h"
#include "og/io/sysrq.h"
#include "og/mem/heap.h"
#include "og/mem/mem_selftest.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm.h"
#include "og/platform/platform.h"
#include "og/platform/power.h"
#include "og/platform/psci.h"
#include "og/platform/smp.h"
#include "og/process/process.h"
#include "og/sched/sched.h"
#include "og/time/timer.h"

extern "C" uintptr_t __stack_chk_guard;
extern "C" uintptr_t __security_cookie;

namespace og {

namespace {

uint64_t g_ipi_count = 0;

void PanicInfo() noexcept {
  sched::Thread* thread = sched::CurrentThread();
  if (thread != nullptr) {
    Log(LogLevel::kError, "panic: thread ");
    Log(LogLevel::kError, thread->name);
    LogDec(LogLevel::kError, "panic: thread cpu", thread->home_cpu);
  }
  process::Process* process = process::Current();
  if (process != nullptr) {
    LogDec(LogLevel::kError, "panic: pid", process->pid);
    Log(LogLevel::kError, "panic: process ");
    Log(LogLevel::kError, process->name);
  }
  LogDec(LogLevel::kError, "panic: heap pages", mem::GetHeapStats().pages);
  LogDec(LogLevel::kError, "panic: frames free", mem::PmmFreeFrames());
  LogDec(LogLevel::kError, "panic: log bytes", log_ring::Size());
}

void SysRqRebootWork(void* /*context*/) noexcept {
  const Status status = power::Reboot();
  LogHex(LogLevel::kError, "sysrq: reboot status",
         static_cast<uint64_t>(status));
}

void SysRqShutdownWork(void* /*context*/) noexcept {
  const Status status = power::Shutdown();
  LogHex(LogLevel::kError, "sysrq: shutdown status",
         static_cast<uint64_t>(status));
}

void SysRqHaltWork(void* /*context*/) noexcept {
  arch::aarch64::DisableInterrupts();
  arch::aarch64::HaltForever();
}

Status UserFault(arch::aarch64::TrapFrame* frame) noexcept {
  process::Process* process = process::Current();
  if (process == nullptr) {
    Panic("trap: user fault without process");
  }

  constexpr uint64_t kEsrEcShift = 26U;
  constexpr uint64_t kEsrEcMask = 0x3FU;
  constexpr uint64_t kEsrIssMask = 0x1FFFFFFULL;
  constexpr uint64_t kDfscMask = 0x3FU;
  constexpr uint64_t kEcDataAbortLower = 0x24U;
  constexpr uint64_t kEcInstructionAbortLower = 0x20U;
  constexpr uint64_t kDfscTranslationFirst = 0x04U;
  constexpr uint64_t kDfscTranslationLast = 0x07U;
  constexpr uint64_t kDfscPermissionFirst = 0x0DU;
  constexpr uint64_t kDfscPermissionLast = 0x0FU;
  constexpr uint64_t kIssWrite = 1ULL << 6U;

  const uint64_t esr = arch::aarch64::ReadEsrEl1();
  const uint64_t ec = (esr >> kEsrEcShift) & kEsrEcMask;
  const uint64_t iss = esr & kEsrIssMask;
  const uint64_t dfsc = iss & kDfscMask;
  const uint64_t far = arch::aarch64::ReadFarEl1();

  if (ec == kEcDataAbortLower && (iss & kIssWrite) != 0U &&
      dfsc >= kDfscPermissionFirst && dfsc <= kDfscPermissionLast) {
    const uint64_t page = far & ~(arch::aarch64::kPageSize - 1U);
    if (IsOk(mem::VmmHandleCow(process->space, page))) {
      return Status::kOk;
    }
  }

  if (ec == kEcDataAbortLower && dfsc >= kDfscTranslationFirst &&
      dfsc <= kDfscTranslationLast) {
    if (IsOk(mem::VmmFaultUserPage(process->space, far))) {
      return Status::kOk;
    }
    if (process::GrowStack(*process, far)) {
      return Status::kOk;
    }
  }

  if (ec == kEcInstructionAbortLower && dfsc >= kDfscTranslationFirst &&
      dfsc <= kDfscTranslationLast &&
      IsOk(mem::VmmFaultUserPage(process->space, far))) {
    return Status::kOk;
  }

  LogDec(LogLevel::kError, "user fault: pid", process->pid);
  LogHex(LogLevel::kError, "user fault: elr", frame->elr);
  LogHex(LogLevel::kError, "user fault: far", far);
  process::Exit(-1);
}

void SysRqHandler(char key) noexcept {
  if (key == 's') {
    Log(LogLevel::kInfo, "sysrq: stats\n");
    LogDec(LogLevel::kInfo, "sysrq: ticks", time::TimerTicks());
    LogDec(LogLevel::kInfo, "sysrq: threads", sched::ThreadCount());
    LogDec(LogLevel::kInfo, "sysrq: processes", process::Count());
    LogDec(LogLevel::kInfo, "sysrq: frames free", mem::PmmFreeFrames());
    LogDec(LogLevel::kInfo, "sysrq: heap pages", mem::GetHeapStats().pages);
    LogDec(LogLevel::kInfo, "sysrq: log bytes", log_ring::Size());
    return;
  }
  if (key == 'b') {
    Log(LogLevel::kInfo, "sysrq: reboot\n");
    (void)events::ScheduleWork(&SysRqRebootWork, nullptr);
    return;
  }
  if (key == 'o') {
    Log(LogLevel::kInfo, "sysrq: shutdown\n");
    (void)events::ScheduleWork(&SysRqShutdownWork, nullptr);
    return;
  }
  if (key == 'h') {
    Log(LogLevel::kInfo, "sysrq: halt\n");
    (void)events::ScheduleWork(&SysRqHaltWork, nullptr);
    return;
  }
  Log(LogLevel::kWarn, "sysrq: unknown key\n");
}

constexpr uint64_t kWatchdogPeriodMs = 10000;
uint64_t g_watchdog_last[4]{};
uint32_t g_watchdog_cpus = 0;
bool g_watchdog_armed = false;

void WatchdogCheck(void* /*context*/) noexcept {
  for (uint32_t cpu = 0; cpu < g_watchdog_cpus; ++cpu) {
    const uint64_t now = sched::CpuHeartbeat(cpu);
    if (g_watchdog_armed && now == g_watchdog_last[cpu]) {
      LogDec(LogLevel::kError, "watchdog: stalled cpu", cpu);
      Panic("watchdog: cpu stalled");
    }
    g_watchdog_last[cpu] = now;
  }
  g_watchdog_armed = true;
  (void)time::ScheduleAfter(kWatchdogPeriodMs, &WatchdogCheck, nullptr);
}

void StartWatchdog() noexcept {
  g_watchdog_cpus = smp::OnlineCount();
  if (g_watchdog_cpus > 4U) {
    g_watchdog_cpus = 4U;
  }
  if (g_watchdog_cpus == 0U) {
    return;
  }
  g_watchdog_armed = false;
  const Result<time::Timeout*> timeout =
    time::ScheduleAfter(kWatchdogPeriodMs, &WatchdogCheck, nullptr);
  if (timeout.is_err()) {
    Log(LogLevel::kWarn, "watchdog: start failed\n");
  }
}

void BroadcastTlbShootdown() noexcept {
  const uint32_t current = smp::CurrentIndex();
  for (uint32_t index = 0; index < smp::CpuCount(); ++index) {
    if (index == current) {
      continue;
    }
    smp::CpuInfo* cpu = smp::CpuAt(index);
    if (cpu != nullptr &&
        __atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE) != 0U) {
      drivers::irq::Gicv3::SendSgi(smp::kIpiIrq, index);
    }
  }
}

Status IpiHandler(void* /*context*/, uint32_t /*irq*/) noexcept {
  __atomic_add_fetch(&g_ipi_count, 1U, __ATOMIC_RELAXED);
  arch::aarch64::InvalidateTlbAllLocal();
  return Status::kOk;
}

void InitSmp() noexcept {
  const Status status = psci::PsciInit(platform::Current().psci_conduit);
  if (IsErr(status)) {
    Log(LogLevel::kWarn, "psci: not available\n");
    return;
  }

  LogDec(LogLevel::kInfo, "psci: version", psci::Version());
  Status smp_status = smp::SmpInit();
  if (IsErr(smp_status)) {
    Log(LogLevel::kError, "smp: init failed\n");
    return;
  }

  smp_status = events::RegisterIrq(smp::kIpiIrq, &IpiHandler, nullptr);
  if (IsErr(smp_status)) {
    Log(LogLevel::kError, "smp: ipi register failed\n");
    return;
  }

  smp_status = smp::StartSecondaries();
  if (IsErr(smp_status)) {
    LogHex(LogLevel::kError, "smp: start status", static_cast<uint64_t>(smp_status));
    return;
  }

  const uint32_t online = smp::OnlineCount();
  LogDec(LogLevel::kInfo, "smp: cpus online", online);
  if (online > 1U) {
    drivers::irq::Gicv3::SendSgi(smp::kIpiIrq, 1);

    const uint64_t ipi_start = arch::aarch64::ReadCntpctEl0();
    const uint64_t ipi_timeout = arch::aarch64::ReadCntfrqEl0() * 2ULL;
    while (__atomic_load_n(&g_ipi_count, __ATOMIC_ACQUIRE) == 0U) {
      if ((arch::aarch64::ReadCntpctEl0() - ipi_start) > ipi_timeout) {
        break;
      }
    }
    const uint64_t ipis = __atomic_load_n(&g_ipi_count, __ATOMIC_ACQUIRE);
    LogDec(LogLevel::kInfo, "smp: ipi count", ipis);
    if (ipis == 0U) {
      Log(LogLevel::kError, "smp: ipi not received\n");
      return;
    }
  } else {
    Log(LogLevel::kInfo, "smp: single core\n");
  }

  mem::SetTlbShootdownHook(&BroadcastTlbShootdown);
  mem::TlbShootdown();
  Log(LogLevel::kInfo, "smp: tlb shootdown ok\n");
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
  SetPanicInfoHook(&PanicInfo);
  io::SetSysRqHandler(&SysRqHandler);

  if (info->acpi_rsdp != 0U) {
    const Status acpi_status = acpi::Init(info->acpi_rsdp);
    if (IsErr(acpi_status)) {
      LogHex(LogLevel::kWarn, "acpi: init status",
             static_cast<uint64_t>(acpi_status));
    } else {
      const acpi::Info& acpi_info = acpi::GetInfo();
      LogDec(LogLevel::kInfo, "acpi: tables", acpi_info.table_count);
      LogDec(LogLevel::kInfo, "acpi: cpus", acpi_info.cpu_count);
    }
  }

  const Status platform_status = platform::PlatformInit(*info);
  if (IsErr(platform_status)) {
    Log(LogLevel::kWarn, "platform: no firmware tables\n");
  } else if (acpi::Ready()) {
    Log(LogLevel::kInfo, "platform: acpi parsed\n");
  } else {
    Log(LogLevel::kInfo, "platform: dtb parsed\n");
  }
  LogDec(LogLevel::kInfo, "platform: cpus", platform::Current().cpu_count);
  LogDec(LogLevel::kInfo, "platform: psci conduit", platform::Current().psci_conduit);

  // Install vectors first so any fault after this point panics with context.
  events::InstallVectorTable();
  events::SetUserFaultHook(&UserFault);

  __stack_chk_guard = arch::aarch64::ReadCntpctEl0() ^ 0x9E3779B97F4A7C15ULL;
  __security_cookie = __stack_chk_guard;

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

  if (arch::aarch64::PanSupported()) {
    arch::aarch64::EnablePan();
    Log(LogLevel::kInfo, "hardening: pan enabled\n");
  } else {
    Log(LogLevel::kInfo, "hardening: pan unsupported\n");
  }

  if (info->text_size != 0U) {
    const Result<mem::PageAttributes> text_attributes =
      mem::VmmQueryAttributes(info->text_base);
    if (text_attributes.is_err()) {
      Panic("hardening: text attributes missing");
    }
    if (!text_attributes.value().executable_el1 || text_attributes.value().writable) {
      Panic("hardening: text is not w^x");
    }
  }

  status = mem::HeapInit();
  if (IsErr(status)) {
    Panic("mem: heap init failed");
  }

  const Result<void*> probe = mem::HeapAlloc(16);
  if (probe.is_err()) {
    Panic("hardening: heap probe failed");
  }
  const Result<mem::PageAttributes> heap_attributes =
    mem::VmmQueryAttributes(reinterpret_cast<uint64_t>(probe.value()));
  if (heap_attributes.is_err()) {
    Panic("hardening: heap attributes missing");
  }
  if (heap_attributes.value().executable_el1 || !heap_attributes.value().writable) {
    Panic("hardening: heap is not n^x");
  }
  (void)mem::HeapFree(probe.value());
  Log(LogLevel::kInfo, "hardening: wx attributes ok\n");

  status = mem::RunSelfTest();
  if (IsErr(status)) {
    Panic("mem: self test failed");
  }

  status = dma::RunDmaSelfTest();
  if (IsErr(status)) {
    Panic("dma: self test failed");
  }

  boot::InitDevices();
  boot::InitFilesystem();

  status = events::WorkQueueInit();
  if (IsErr(status)) {
    Panic("events: workqueue init failed");
  }
  status = events::RunEventSelfTest();
  if (IsErr(status)) {
    Panic("events: self test failed");
  }

  drivers::irq::Gicv3 gic;
  status = gic.init(platform::Current().gic_distributor.base,
                    platform::Current().gic_redistributor.base);
  if (IsErr(status)) {
    Panic("gic: init failed");
  }
  events::SetInterruptController(drivers::irq::MakeController(gic));
  Log(LogLevel::kInfo, "gic: ready\n");

  drivers::uart::Pl011* uart = drivers::uart::Active();
  if (uart != nullptr) {
    const Status rx_status =
      uart->enable_receive(drivers::uart::kUartIrq, platform::Current().uart_clock, 115200);
    if (IsErr(rx_status)) {
      Log(LogLevel::kWarn, "uart: rx init failed\n");
    } else {
      const Status irq_status = gic.enable_irq(drivers::uart::kUartIrq);
      if (IsErr(irq_status)) {
        Log(LogLevel::kWarn, "uart: rx irq enable failed\n");
      }
    }
  }

  status = time::TimerInit();
  if (IsErr(status)) {
    Panic("timer: init failed");
  }
  status = gic.enable_irq(time::kTimerIrq);
  if (IsErr(status)) {
    Panic("timer: irq enable failed");
  }
  LogDec(LogLevel::kInfo, "timer: frequency", time::TimerFrequency());

  status = time::TimerStartPeriodic(sched::kTickHz);
  if (IsErr(status)) {
    Panic("timer: start failed");
  }

  boot::RunTimerSelfTest();

  status = sched::SchedulerInit();
  if (IsErr(status)) {
    LogHex(LogLevel::kError, "sched: init status", static_cast<uint64_t>(status));
    Panic("sched: init failed");
  }
  status = process::Init();
  if (IsErr(status)) {
    Panic("process: init failed");
  }

  InitSmp();
  power::SetHandlers(&platform::Shutdown, &platform::Reboot);
  boot::RunSchedulerSelfTest();
  StartWatchdog();
  boot::RunAsidSelfTest();
  boot::RunAddressSpaceSelfTest();
  boot::RunUserSelfTest();

  (void)PrintLn("kernel: halt");
  arch::aarch64::HaltForever();
}

}  // namespace og

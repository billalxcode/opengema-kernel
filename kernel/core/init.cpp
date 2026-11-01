#include "og/boot/init.h"

#include "memory.h"
#include "og/acpi/acpi.h"
#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/core/bootinfo.h"
#include "og/core/log.h"
#include "og/core/panic.h"
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
#include "og/mem/heap.h"
#include "og/mem/mem_selftest.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm.h"
#include "og/platform/platform.h"
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

Status IpiHandler(void* /*context*/, uint32_t /*irq*/) noexcept {
  __atomic_add_fetch(&g_ipi_count, 1U, __ATOMIC_RELAXED);
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

  LogDec(LogLevel::kInfo, "smp: cpus online", smp::OnlineCount());
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
  }
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
  boot::RunSchedulerSelfTest();
  boot::RunAddressSpaceSelfTest();
  boot::RunUserSelfTest();

  (void)PrintLn("kernel: halt");
  arch::aarch64::HaltForever();
}

}  // namespace og

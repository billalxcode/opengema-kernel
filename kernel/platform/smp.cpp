#include "og/platform/smp.h"

#include "memory.h"
#include "og/arch/cpu.h"
#include "og/arch/mmu.h"
#include "og/arch/registers.h"
#include "og/core/log.h"
#include "og/drivers/gic.h"
#include "og/events/irq.h"
#include "og/events/trap.h"
#include "og/io/console.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm.h"
#include "og/sched/sched.h"
#include "og/time/timer.h"
#include "og/platform/platform.h"
#include "og/platform/psci.h"

extern "C" void SecondaryEntry();

namespace og::smp {

namespace {

constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kOnlineTimeoutSeconds = 5;

CpuInfo g_cpus[kMaxCpus];
uint32_t g_cpu_count = 0;

[[nodiscard]] uint64_t RedistributorFor(uint32_t index) noexcept {
  return platform::Current().gic_redistributor.base +
         (static_cast<uint64_t>(index) * drivers::irq::kRedistributorStride);
}

}  // namespace

CpuInfo* FindCpuFor(uint64_t mpidr) noexcept {
  for (uint32_t index = 0; index < g_cpu_count; ++index) {
    if (g_cpus[index].mpidr == mpidr) {
      return &g_cpus[index];
    }
  }
  return nullptr;
}

CpuInfo* CpuAt(uint32_t index) noexcept {
  return index < g_cpu_count ? &g_cpus[index] : nullptr;
}

Status SmpInit() noexcept {
  const platform::Platform& board = platform::Current();
  g_cpu_count = board.cpu_count > kMaxCpus ? kMaxCpus : board.cpu_count;
  if (g_cpu_count == 0) {
    return Status::kInvalidArg;
  }

  for (uint32_t index = 0; index < g_cpu_count; ++index) {
    g_cpus[index].mpidr = index;
    g_cpus[index].index = index;
    g_cpus[index].online = 0;
    g_cpus[index].stack_top = 0;
    if (index == 0) {
      continue;
    }
    const Result<uint64_t> memory = mem::PmmAllocPages(kKernelStackPages);
    if (memory.is_err()) {
      return memory.status();
    }
    g_cpus[index].stack_top = memory.value() + (kKernelStackPages * kPageSize);
  }

  arch::aarch64::WriteTpidrEl1(reinterpret_cast<uint64_t>(&g_cpus[0]));
  __atomic_store_n(&g_cpus[0].online, 1U, __ATOMIC_RELEASE);
  return Status::kOk;
}

Status StartSecondaries() noexcept {
  if (g_cpu_count <= 1U) {
    return Status::kOk;
  }

  // Arm the IPI on every redistributor before the cores exist.
  for (uint32_t index = 0; index < g_cpu_count; ++index) {
    const Status status = drivers::irq::Gicv3::EnablePpiAt(RedistributorFor(index), kIpiIrq);
    if (IsErr(status)) {
      return status;
    }
  }

  for (uint32_t index = 1; index < g_cpu_count; ++index) {
    const Result<uint64_t> started = psci::CpuOn(
      g_cpus[index].mpidr, reinterpret_cast<uint64_t>(&SecondaryEntry),
      g_cpus[index].stack_top);
    if (started.is_err()) {
      LogDec(LogLevel::kError, "smp: cpu failed", index);
      return started.status();
    }
  }

  const uint64_t start = arch::aarch64::ReadCntpctEl0();
  const uint64_t timeout = arch::aarch64::ReadCntfrqEl0() * kOnlineTimeoutSeconds;
  while (OnlineCount() < g_cpu_count) {
    if ((arch::aarch64::ReadCntpctEl0() - start) > timeout) {
      return Status::kTimeout;
    }
  }
  return Status::kOk;
}

uint32_t OnlineCount() noexcept {
  uint32_t count = 0;
  for (uint32_t index = 0; index < g_cpu_count; ++index) {
    if (__atomic_load_n(&g_cpus[index].online, __ATOMIC_ACQUIRE) != 0U) {
      ++count;
    }
  }
  return count;
}

uint32_t CpuCount() noexcept {
  return g_cpu_count;
}

CpuInfo* CurrentCpu() noexcept {
  return reinterpret_cast<CpuInfo*>(arch::aarch64::ReadTpidrEl1());
}

uint32_t CurrentIndex() noexcept {
  CpuInfo* cpu = CurrentCpu();
  return cpu == nullptr ? 0U : cpu->index;
}

}  // namespace og::smp

extern "C" void SecondaryMain(uint64_t stack_top) noexcept {
  (void)stack_top;

  const og::Status activated = og::arch::aarch64::ActivateAddressSpace(
    og::mem::VmmRootAddress());
  if (og::IsErr(activated)) {
    for (;;) {
      og::arch::aarch64::WaitForInterrupt();
    }
  }
  og::events::InstallVectorTable();

  const uint64_t mpidr = og::arch::aarch64::CpuId() & 0xFFU;
  og::smp::CpuInfo* self = og::smp::FindCpuFor(mpidr);
  if (self == nullptr) {
    for (;;) {
      og::arch::aarch64::WaitForInterrupt();
    }
  }

  og::arch::aarch64::WriteTpidrEl1(reinterpret_cast<uint64_t>(self));

  const og::platform::Platform& board = og::platform::Current();
  const uint64_t redistributor = board.gic_redistributor.base +
                                 (static_cast<uint64_t>(self->index) *
                                  og::drivers::irq::kRedistributorStride);
  og::drivers::irq::Gicv3 gic;
  if (og::IsErr(gic.init(board.gic_distributor.base, redistributor))) {
    og::Log(og::LogLevel::kError, "smp: secondary gic failed\n");
    for (;;) {
      og::arch::aarch64::WaitForInterrupt();
    }
  }
  (void)gic.enable_irq(og::smp::kIpiIrq);
  if (og::IsErr(gic.enable_irq(og::time::kTimerIrq))) {
    og::Log(og::LogLevel::kError, "smp: secondary timer irq failed\n");
    for (;;) {
      og::arch::aarch64::WaitForInterrupt();
    }
  }

  if (og::IsErr(og::sched::SchedulerInitCpu())) {
    og::Log(og::LogLevel::kError, "smp: secondary scheduler failed\n");
    for (;;) {
      og::arch::aarch64::WaitForInterrupt();
    }
  }
  if (og::IsErr(og::time::TimerStartPeriodic(og::sched::kTickHz))) {
    og::Log(og::LogLevel::kError, "smp: secondary timer start failed\n");
    for (;;) {
      og::arch::aarch64::WaitForInterrupt();
    }
  }

  __atomic_store_n(&self->online, 1U, __ATOMIC_RELEASE);
  og::LogDec(og::LogLevel::kInfo, "smp: cpu online", self->index);
  og::arch::aarch64::EnableIrq();

  for (;;) {
    og::arch::aarch64::WaitForInterrupt();
  }
}

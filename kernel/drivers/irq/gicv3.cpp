#include "og/drivers/gic.h"

#include "og/arch/barriers.h"
#include "og/arch/registers.h"
#include "og/time/delay.h"

namespace og::drivers::irq {

namespace {

constexpr uint64_t kDistributorSize = 0x10000;
constexpr uint64_t kRedistributorSize = 0x20000;

constexpr uint64_t kDistributorControl = 0x0000;
constexpr uint64_t kDistributorGroup = 0x0080;
constexpr uint64_t kDistributorEnable = 0x0100;
constexpr uint64_t kDistributorDisable = 0x0180;
constexpr uint64_t kDistributorPriority = 0x0400;

constexpr uint64_t kRedistributorWake = 0x0014;
constexpr uint64_t kRedistributorSgiFrame = 0x10000;
constexpr uint64_t kRedistributorGroup0 = kRedistributorSgiFrame + 0x0080;
constexpr uint64_t kRedistributorEnable0 = kRedistributorSgiFrame + 0x0100;
constexpr uint64_t kRedistributorDisable0 = kRedistributorSgiFrame + 0x0180;
constexpr uint64_t kRedistributorPrioritySgi = kRedistributorSgiFrame + 0x0400;

constexpr uint32_t kControlEnableGroup1 = 1U << 1U;
constexpr uint32_t kControlAffinityRouting = 1U << 4U;
constexpr uint32_t kWakeProcessorSleep = 1U << 1U;
constexpr uint32_t kWakeChildrenAsleep = 1U << 2U;
constexpr uint8_t kDefaultPriority = 0xA0;
constexpr uint64_t kIccSreEnable = 1ULL;
constexpr uint64_t kIccCtlrEoiMode = 1ULL << 1U;
constexpr uint64_t kIccPmrAllowAll = 0xFFULL;
constexpr uint64_t kIccGroup1Enable = 1ULL;
constexpr uint64_t kIarIntIdMask = 0xFFFFFFULL;

uint32_t ControllerAcknowledge(void* context) noexcept {
  if (context == nullptr) {
    return og::events::kSpuriousIrq;
  }
  return static_cast<Gicv3*>(context)->acknowledge();
}

void ControllerEnd(void* context, uint32_t intid) noexcept {
  if (context == nullptr) {
    return;
  }
  static_cast<Gicv3*>(context)->end(intid);
}

}  // namespace

Status Gicv3::init(uint64_t distributor_base, uint64_t redistributor_base) noexcept {
  if (distributor_base == 0) {
    return Status::kInvalidArg;
  }
  if (redistributor_base == 0) {
    return Status::kInvalidArg;
  }

  distributor_ = io::MmioRegion(distributor_base, kDistributorSize);
  redistributor_ = io::MmioRegion(redistributor_base, kRedistributorSize);

  // Wake this core's redistributor and wait for children to sleep.
  const Result<uint32_t> wake = redistributor_.Read32(kRedistributorWake);
  if (wake.is_err()) {
    return wake.status();
  }
  const uint32_t wake_value = wake.value() & ~kWakeProcessorSleep;
  const Status wake_status = redistributor_.Write32(kRedistributorWake, wake_value);
  if (IsErr(wake_status)) {
    return wake_status;
  }
  for (uint32_t attempt = 0; attempt < 100U; ++attempt) {
    const Result<uint32_t> state = redistributor_.Read32(kRedistributorWake);
    if (state.is_err()) {
      return state.status();
    }
    if ((state.value() & kWakeChildrenAsleep) == 0U) {
      break;
    }
    time::DelayUs(100);
  }

  // System register CPU interface.
  uint64_t sre = arch::aarch64::ReadIccSreEl1();
  sre |= kIccSreEnable;
  arch::aarch64::WriteIccSreEl1(sre);
  arch::aarch64::InstructionSync();
  if ((arch::aarch64::ReadIccSreEl1() & kIccSreEnable) == 0U) {
    return Status::kUnsupported;
  }

  const uint64_t ctlr = arch::aarch64::ReadIccCtlrEl1();
  arch::aarch64::WriteIccCtlrEl1(ctlr & ~kIccCtlrEoiMode);
  arch::aarch64::WriteIccPmrEl1(kIccPmrAllowAll);
  arch::aarch64::WriteIccIgrpen1El1(kIccGroup1Enable);
  arch::aarch64::InstructionSync();

  // Distributor: affinity routing and group 1 non secure.
  const Result<uint32_t> control = distributor_.Read32(kDistributorControl);
  if (control.is_err()) {
    return control.status();
  }
  const uint32_t enabled = control.value() | kControlAffinityRouting | kControlEnableGroup1;
  const Status control_status = distributor_.Write32(kDistributorControl, enabled);
  if (IsErr(control_status)) {
    return control_status;
  }
  distributor_.Sync();

  ready_ = true;
  return Status::kOk;
}

Status Gicv3::enable_irq(uint32_t intid) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (intid >= 1020U) {
    return Status::kInvalidArg;
  }

  if (intid < 32U) {
    const uint32_t bit = 1U << intid;
    const Status group = redistributor_.Write32(kRedistributorGroup0, bit);
    if (IsErr(group)) {
      return group;
    }
    const Status priority =
      redistributor_.Write8(kRedistributorPrioritySgi + intid, kDefaultPriority);
    if (IsErr(priority)) {
      return priority;
    }
    return redistributor_.Write32(kRedistributorEnable0, bit);
  }

  const uint64_t index = intid / 32U;
  const uint32_t bit = 1U << (intid % 32U);
  const Status group = distributor_.Write32(kDistributorGroup + (index * 4U), bit);
  if (IsErr(group)) {
    return group;
  }
  const Status priority = distributor_.Write8(kDistributorPriority + intid, kDefaultPriority);
  if (IsErr(priority)) {
    return priority;
  }
  return distributor_.Write32(kDistributorEnable + (index * 4U), bit);
}

Status Gicv3::disable_irq(uint32_t intid) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (intid >= 1020U) {
    return Status::kInvalidArg;
  }
  if (intid < 32U) {
    const uint32_t bit = 1U << intid;
    return redistributor_.Write32(kRedistributorDisable0, bit);
  }
  const uint64_t index = intid / 32U;
  const uint32_t bit = 1U << (intid % 32U);
  return distributor_.Write32(kDistributorDisable + (index * 4U), bit);
}

uint32_t Gicv3::acknowledge() noexcept {
  if (!ready_) {
    return events::kSpuriousIrq;
  }
  const uint64_t value = arch::aarch64::ReadIccIar1El1();
  return static_cast<uint32_t>(value & kIarIntIdMask);
}

void Gicv3::end(uint32_t intid) noexcept {
  if (!ready_) {
    return;
  }
  arch::aarch64::WriteIccEoir1El1(static_cast<uint64_t>(intid));
}

Status Gicv3::EnablePpiAt(uint64_t redistributor_base, uint32_t intid) noexcept {
  if (redistributor_base == 0) {
    return Status::kInvalidArg;
  }
  if (intid >= 32U) {
    return Status::kInvalidArg;
  }
  io::MmioRegion frame(redistributor_base, kRedistributorSize);
  const uint32_t bit = 1U << intid;
  Status status = frame.Write32(kRedistributorGroup0, bit);
  if (IsErr(status)) {
    return status;
  }
  status = frame.Write8(kRedistributorPrioritySgi + intid, kDefaultPriority);
  if (IsErr(status)) {
    return status;
  }
  return frame.Write32(kRedistributorEnable0, bit);
}

void Gicv3::SendSgi(uint32_t intid, uint64_t target_aff0) noexcept {
  if (intid > 15U) {
    return;
  }
  const uint64_t value = (static_cast<uint64_t>(intid) << 24U) | (1ULL << target_aff0);
  arch::aarch64::WriteIccSgi1rEl1(value);
}

events::InterruptController MakeController(Gicv3& gic) noexcept {
  if (!gic.is_ready()) {
    return events::InterruptController{};
  }
  events::InterruptController controller{};
  controller.acknowledge = &ControllerAcknowledge;
  controller.end = &ControllerEnd;
  controller.context = &gic;
  return controller;
}

}  // namespace og::drivers::irq

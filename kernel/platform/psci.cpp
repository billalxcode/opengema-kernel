#include "og/psci.h"

namespace og::psci {

namespace {

constexpr uint64_t kFunctionVersion = 0x84000000ULL;
constexpr uint64_t kFunctionCpuOn64 = 0xC4000003ULL;
constexpr uint64_t kSuccess = 0;
constexpr uint64_t kNotSupported = static_cast<uint64_t>(-1);

uint32_t g_conduit = 0;  // 0 = hvc, 1 = smc

[[nodiscard]] uint64_t CallHvc(uint64_t function, uint64_t arg1, uint64_t arg2,
                               uint64_t arg3) noexcept {
  register uint64_t x0 __asm__("x0") = function;
  register uint64_t x1 __asm__("x1") = arg1;
  register uint64_t x2 __asm__("x2") = arg2;
  register uint64_t x3 __asm__("x3") = arg3;
  __asm__ volatile("hvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
  return x0;
}

[[nodiscard]] uint64_t CallSmc(uint64_t function, uint64_t arg1, uint64_t arg2,
                               uint64_t arg3) noexcept {
  register uint64_t x0 __asm__("x0") = function;
  register uint64_t x1 __asm__("x1") = arg1;
  register uint64_t x2 __asm__("x2") = arg2;
  register uint64_t x3 __asm__("x3") = arg3;
  __asm__ volatile("smc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
  return x0;
}

[[nodiscard]] uint64_t Call(uint64_t function, uint64_t arg1, uint64_t arg2,
                            uint64_t arg3) noexcept {
  if (g_conduit == 1U) {
    return CallSmc(function, arg1, arg2, arg3);
  }
  return CallHvc(function, arg1, arg2, arg3);
}

[[nodiscard]] bool PlausibleVersion(uint64_t value) noexcept {
  if (value == kNotSupported) {
    return false;
  }
  const uint64_t major = (value >> 16U) & 0xFFFFU;
  return major >= 0U && major <= 2U;
}

}  // namespace

Status PsciInit() noexcept {
  const uint64_t hvc_version = CallHvc(kFunctionVersion, 0, 0, 0);
  if (PlausibleVersion(hvc_version)) {
    g_conduit = 0;
    return Status::kOk;
  }

  const uint64_t smc_version = CallSmc(kFunctionVersion, 0, 0, 0);
  if (PlausibleVersion(smc_version)) {
    g_conduit = 1;
    return Status::kOk;
  }

  return Status::kUnsupported;
}

Result<uint64_t> CpuOn(uint64_t mpidr, uint64_t entry_address, uint64_t context) noexcept {
  const uint64_t result = Call(kFunctionCpuOn64, mpidr, entry_address, context);
  if (result != kSuccess) {
    return Result<uint64_t>::Err(Status::kIoError);
  }
  return Result<uint64_t>::Ok(result);
}

uint32_t Version() noexcept {
  return static_cast<uint32_t>(Call(kFunctionVersion, 0, 0, 0) & 0xFFFFFFFFULL);
}

}  // namespace og::psci

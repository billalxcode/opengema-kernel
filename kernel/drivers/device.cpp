#include "og/device.h"

#include "memory.h"
#include "og/arch/spinlock.h"

namespace og::drivers {

namespace {

Device g_devices[kMaxDevices];
uint32_t g_count = 0;
bool g_ready = false;
arch::aarch64::IrqSpinlock g_lock;

[[nodiscard]] bool SameName(const char* lhs, const char* rhs) noexcept {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return mem::Same(lhs, rhs);
}

}  // namespace

Status DeviceInit() noexcept {
  mem::Zero(g_devices, sizeof(g_devices));
  g_count = 0;
  g_ready = true;
  return Status::kOk;
}

Status RegisterDevice(const Device& device) noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (device.name == nullptr) {
    return Status::kInvalidArg;
  }

  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  if (g_count >= kMaxDevices) {
    return Status::kNoMem;
  }
  g_devices[g_count] = device;
  ++g_count;
  return Status::kOk;
}

Result<Device> FindDeviceByName(const char* name) noexcept {
  if (!g_ready) {
    return Result<Device>::Err(Status::kNotInit);
  }
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  for (uint32_t index = 0; index < g_count; ++index) {
    if (SameName(g_devices[index].name, name)) {
      return Result<Device>::Ok(g_devices[index]);
    }
  }
  return Result<Device>::Err(Status::kNotFound);
}

Result<Device> FindFirstDevice(DeviceClass device_class) noexcept {
  if (!g_ready) {
    return Result<Device>::Err(Status::kNotInit);
  }
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  for (uint32_t index = 0; index < g_count; ++index) {
    if (g_devices[index].device_class == device_class) {
      return Result<Device>::Ok(g_devices[index]);
    }
  }
  return Result<Device>::Err(Status::kNotFound);
}

uint32_t DeviceCount() noexcept {
  return g_count;
}

}  // namespace og::drivers

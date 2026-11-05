#pragma once

// Canonical owner of the device registry. Drivers register themselves,
// upper layers look devices up by name or class.

#include <cstdint>

#include "og/core/status.h"

namespace og::drivers {

enum class DeviceClass : uint32_t {
  kUnknown = 0,
  kBlock = 1,
  kNet = 2,
  kDisplay = 3,
  kSerial = 4,
  kInterrupt = 5,
  kInput = 6,
};

struct Device {
  const char* name;
  DeviceClass device_class;
  uint64_t mmio_base;
  uint32_t irq;
  uint64_t clock_hz;
  uint64_t capacity;
  void* driver_data;
};

constexpr uint32_t kMaxDevices = 32;

[[nodiscard]] Status DeviceInit() noexcept;
[[nodiscard]] Status RegisterDevice(const Device& device) noexcept;

[[nodiscard]] Result<Device> FindDeviceByName(const char* name) noexcept;
[[nodiscard]] Result<Device> FindFirstDevice(DeviceClass device_class) noexcept;
[[nodiscard]] Result<Device> DeviceAt(uint32_t index) noexcept;
[[nodiscard]] uint32_t DeviceCount() noexcept;

}  // namespace og::drivers

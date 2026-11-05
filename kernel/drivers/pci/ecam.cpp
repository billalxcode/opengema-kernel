#include "og/drivers/pci.h"

#include "og/io/mmio.h"
#include "og/mem/vmm.h"

namespace og::drivers::pci {

namespace {

constexpr uint16_t kConfigSpaceSize = 4096;
constexpr uint16_t kRegisterVendorId = 0x00;
constexpr uint16_t kRegisterDeviceId = 0x02;
constexpr uint16_t kRegisterCommand = 0x04;
constexpr uint16_t kRegisterBar0 = 0x10;
constexpr uint16_t kRegisterStatus = 0x06;
constexpr uint16_t kRegisterCapabilities = 0x34;
constexpr uint16_t kRegisterInterruptLine = 0x3C;
constexpr uint16_t kRegisterInterruptPin = 0x3D;

constexpr uint16_t kStatusCapabilities = 1U << 4U;
constexpr uint8_t kCapabilityMsiX = 0x11;
constexpr uint32_t kCapabilityScanLimit = 48U;

constexpr uint16_t kCommandMemorySpace = 1U << 1U;
constexpr uint16_t kCommandBusMaster = 1U << 2U;
// The driver polls virtio queues, so the legacy INTx line must stay quiet.
constexpr uint16_t kCommandInterruptDisable = 1U << 10U;
constexpr uint8_t kBarCount = 6;

io::MmioRegion g_ecam;
uint32_t g_bus_count = 0;
bool g_ready = false;

[[nodiscard]] bool ValidAddress(Address address) noexcept {
  if (!g_ready) {
    return false;
  }
  if (address.bus >= g_bus_count) {
    return false;
  }
  if (address.device >= 32U) {
    return false;
  }
  if (address.function >= 8U) {
    return false;
  }
  return true;
}

[[nodiscard]] uint64_t RegisterOffset(Address address, uint16_t offset) noexcept {
  const uint64_t bus = static_cast<uint64_t>(address.bus) << 20U;
  const uint64_t device = static_cast<uint64_t>(address.device) << 15U;
  const uint64_t function = static_cast<uint64_t>(address.function) << 12U;
  return bus | device | function | offset;
}

}  // namespace

Status PciInit(uint64_t ecam_base, uint64_t ecam_size) noexcept {
  if (ecam_base == 0) {
    return Status::kInvalidArg;
  }
  if (ecam_size < 0x100000ULL) {
    return Status::kInvalidArg;
  }
  g_ecam = io::MmioRegion(ecam_base, ecam_size);
  // One bus is 32 devices times 8 functions times 4 KiB.
  g_bus_count = static_cast<uint32_t>(ecam_size / 0x100000ULL);
  if (g_bus_count > 256U) {
    g_bus_count = 256U;
  }
  const Status mapping = mem::VmmMapDevice(ecam_base, ecam_size);
  if (IsErr(mapping)) {
    return mapping;
  }
  g_ready = true;
  return Status::kOk;
}

bool PciReady() noexcept {
  return g_ready;
}

Result<uint8_t> Read8(Address address, uint16_t offset) noexcept {
  if (!ValidAddress(address)) {
    return Result<uint8_t>::Err(Status::kInvalidArg);
  }
  if (offset >= kConfigSpaceSize) {
    return Result<uint8_t>::Err(Status::kInvalidArg);
  }
  return g_ecam.Read8(RegisterOffset(address, offset));
}

Result<uint16_t> Read16(Address address, uint16_t offset) noexcept {
  if (!ValidAddress(address)) {
    return Result<uint16_t>::Err(Status::kInvalidArg);
  }
  if (offset >= (kConfigSpaceSize - 1)) {
    return Result<uint16_t>::Err(Status::kInvalidArg);
  }
  if ((offset % 2U) != 0U) {
    return Result<uint16_t>::Err(Status::kUnaligned);
  }
  return g_ecam.Read16(RegisterOffset(address, offset));
}

Result<uint32_t> Read32(Address address, uint16_t offset) noexcept {
  if (!ValidAddress(address)) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }
  if (offset >= (kConfigSpaceSize - 3)) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }
  if ((offset % 4U) != 0U) {
    return Result<uint32_t>::Err(Status::kUnaligned);
  }
  return g_ecam.Read32(RegisterOffset(address, offset));
}

Status Write16(Address address, uint16_t offset, uint16_t value) noexcept {
  if (!ValidAddress(address)) {
    return Status::kInvalidArg;
  }
  if (offset >= (kConfigSpaceSize - 1)) {
    return Status::kInvalidArg;
  }
  if ((offset % 2U) != 0U) {
    return Status::kUnaligned;
  }
  return g_ecam.Write16(RegisterOffset(address, offset), value);
}

Status Write32(Address address, uint16_t offset, uint32_t value) noexcept {  if (!ValidAddress(address)) {
    return Status::kInvalidArg;
  }
  if (offset >= (kConfigSpaceSize - 3)) {
    return Status::kInvalidArg;
  }
  if ((offset % 4U) != 0U) {
    return Status::kUnaligned;
  }
  return g_ecam.Write32(RegisterOffset(address, offset), value);
}

Result<Info> Probe(Address address) noexcept {
  const Result<uint16_t> vendor = Read16(address, kRegisterVendorId);
  if (vendor.is_err()) {
    return Result<Info>::Err(vendor.status());
  }
  if (vendor.value() == 0xFFFFU) {
    return Result<Info>::Err(Status::kNotFound);
  }

  const Result<uint16_t> device = Read16(address, kRegisterDeviceId);
  if (device.is_err()) {
    return Result<Info>::Err(device.status());
  }
  const Result<uint16_t> command = Read16(address, kRegisterCommand);
  if (command.is_err()) {
    return Result<Info>::Err(command.status());
  }
  const Result<uint32_t> class_register = Read32(address, 0x08);
  if (class_register.is_err()) {
    return Result<Info>::Err(class_register.status());
  }

  Info info{};
  info.address = address;
  info.vendor_id = vendor.value();
  info.device_id = device.value();
  info.command = command.value();
  info.class_code = static_cast<uint8_t>((class_register.value() >> 24U) & 0xFFU);
  info.subclass = static_cast<uint8_t>((class_register.value() >> 16U) & 0xFFU);
  info.programming_interface = static_cast<uint8_t>((class_register.value() >> 8U) & 0xFFU);

  for (uint8_t index = 0; index < kBarCount; ++index) {
    const uint16_t offset = static_cast<uint16_t>(kRegisterBar0 + (index * 4U));
    const Result<uint32_t> bar = Read32(address, offset);
    if (bar.is_err()) {
      return Result<Info>::Err(bar.status());
    }
    info.bar[index] = bar.value();
  }

  return Result<Info>::Ok(info);
}

Result<MsiXInfo> ReadMsiX(Address address) noexcept {
  const Result<uint16_t> status = Read16(address, kRegisterStatus);
  if (status.is_err()) {
    return Result<MsiXInfo>::Err(status.status());
  }
  if ((status.value() & kStatusCapabilities) == 0U) {
    return Result<MsiXInfo>::Ok(MsiXInfo{});
  }
  const Result<uint8_t> first = Read8(address, kRegisterCapabilities);
  if (first.is_err()) {
    return Result<MsiXInfo>::Err(first.status());
  }

  uint8_t offset = static_cast<uint8_t>(first.value() & 0xFCU);
  for (uint32_t count = 0; count < kCapabilityScanLimit && offset >= 0x40U;
       ++count) {
    const Result<uint8_t> id = Read8(address, offset);
    if (id.is_err()) {
      return Result<MsiXInfo>::Err(id.status());
    }
    if (id.value() == kCapabilityMsiX) {
      const Result<uint16_t> control = Read16(
        address, static_cast<uint16_t>(offset + 2U));
      if (control.is_err()) {
        return Result<MsiXInfo>::Err(control.status());
      }
      const Result<uint32_t> table = Read32(
        address, static_cast<uint16_t>(offset + 4U));
      if (table.is_err()) {
        return Result<MsiXInfo>::Err(table.status());
      }
      MsiXInfo info{};
      info.present = true;
      info.enabled = (control.value() & 0x8000U) != 0U;
      info.vectors = static_cast<uint16_t>((control.value() & 0x7FFU) + 1U);
      info.bar = static_cast<uint8_t>(table.value() & 0x7U);
      info.table_offset = table.value() & ~0x7U;
      return Result<MsiXInfo>::Ok(info);
    }
    const Result<uint8_t> next = Read8(
      address, static_cast<uint16_t>(offset + 1U));
    if (next.is_err()) {
      return Result<MsiXInfo>::Err(next.status());
    }
    offset = static_cast<uint8_t>(next.value() & 0xFCU);
  }
  return Result<MsiXInfo>::Ok(MsiXInfo{});
}

Result<uint8_t> LegacyIrq(Address address) noexcept {
  const Result<uint8_t> pin = Read8(address, kRegisterInterruptPin);
  if (pin.is_err()) {
    return Result<uint8_t>::Err(pin.status());
  }
  if (pin.value() == 0U || pin.value() == 0xFFU) {
    return Result<uint8_t>::Ok(0U);
  }
  const Result<uint8_t> line = Read8(address, kRegisterInterruptLine);
  if (line.is_err()) {
    return Result<uint8_t>::Err(line.status());
  }
  if (line.value() == 0xFFU) {
    return Result<uint8_t>::Ok(0U);
  }
  return Result<uint8_t>::Ok(line.value());
}

Result<Info> FindByVendorDevice(uint16_t vendor_id, uint16_t device_id) noexcept {
  if (!g_ready) {
    return Result<Info>::Err(Status::kNotInit);
  }
  for (uint32_t bus = 0; bus < g_bus_count; ++bus) {
    for (uint32_t device = 0; device < 32U; ++device) {
      for (uint32_t function = 0; function < 8U; ++function) {
        Address address{static_cast<uint8_t>(bus), static_cast<uint8_t>(device),
                        static_cast<uint8_t>(function)};
        const Result<Info> candidate = Probe(address);
        if (candidate.is_err()) {
          continue;
        }
        if (candidate.value().vendor_id != vendor_id) {
          continue;
        }
        if (candidate.value().device_id != device_id) {
          continue;
        }
        return candidate;
      }
    }
  }
  return Result<Info>::Err(Status::kNotFound);
}

Result<Info> FindNth(MatchFunction match, uint32_t index) noexcept {
  if (!g_ready) {
    return Result<Info>::Err(Status::kNotInit);
  }
  if (match == nullptr) {
    return Result<Info>::Err(Status::kInvalidArg);
  }
  uint32_t seen = 0;
  for (uint32_t bus = 0; bus < g_bus_count; ++bus) {
    for (uint32_t device = 0; device < 32U; ++device) {
      for (uint32_t function = 0; function < 8U; ++function) {
        Address address{static_cast<uint8_t>(bus), static_cast<uint8_t>(device),
                        static_cast<uint8_t>(function)};
        const Result<Info> candidate = Probe(address);
        if (candidate.is_err()) {
          continue;
        }
        if (!match(candidate.value())) {
          continue;
        }
        if (seen == index) {
          return candidate;
        }
        ++seen;
      }
    }
  }
  return Result<Info>::Err(Status::kNotFound);
}

Result<Info> FindFirst(MatchFunction match) noexcept {
  return FindNth(match, 0);
}

Status EnableDevice(Address address) noexcept {
  const Result<uint16_t> current = Read16(address, kRegisterCommand);
  if (current.is_err()) {
    return current.status();
  }
  const uint16_t enable = static_cast<uint16_t>(kCommandMemorySpace | kCommandBusMaster |
                                                kCommandInterruptDisable);
  const uint16_t wanted = static_cast<uint16_t>(current.value() | enable);
  if (wanted == current.value()) {
    return Status::kOk;
  }
  return Write16(address, kRegisterCommand, wanted);
}

Result<uint64_t> BarBase(const Info& info, uint8_t bar_index) noexcept {
  if (bar_index >= kBarCount) {
    return Result<uint64_t>::Err(Status::kInvalidArg);
  }
  const uint32_t value = info.bar[bar_index];
  if ((value & 1U) != 0U) {
    return Result<uint64_t>::Err(Status::kUnsupported);
  }

  uint64_t base = static_cast<uint64_t>(value & ~0xFULL);
  const uint32_t kind = (value >> 1U) & 0x3U;
  if (kind == 0x2U) {
    if ((bar_index + 1U) >= kBarCount) {
      return Result<uint64_t>::Err(Status::kInvalidArg);
    }
    base |= static_cast<uint64_t>(info.bar[bar_index + 1U]) << 32U;
  }
  if (base == 0) {
    return Result<uint64_t>::Err(Status::kNotFound);
  }
  return Result<uint64_t>::Ok(base);
}

}  // namespace og::drivers::pci

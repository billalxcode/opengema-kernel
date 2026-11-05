#pragma once

// Canonical owner of PCIe configuration space access through ECAM.
// BARs are assigned by firmware; this layer only reads and validates them.

#include <cstdint>

#include "og/core/status.h"

namespace og::drivers::pci {

struct Address {
  uint8_t bus;
  uint8_t device;
  uint8_t function;
};

struct Info {
  Address address;
  uint16_t vendor_id;
  uint16_t device_id;
  uint16_t command;
  uint8_t class_code;
  uint8_t subclass;
  uint8_t programming_interface;
  uint32_t bar[6];
};

struct MsiXInfo {
  bool present;
  bool enabled;
  uint16_t vectors;
  uint8_t bar;
  uint32_t table_offset;
};

constexpr uint16_t kVendorRedHat = 0x1AF4;

[[nodiscard]] Status PciInit(uint64_t ecam_base, uint64_t ecam_size) noexcept;
[[nodiscard]] bool PciReady() noexcept;

[[nodiscard]] Result<uint8_t> Read8(Address address, uint16_t offset) noexcept;
[[nodiscard]] Result<uint16_t> Read16(Address address, uint16_t offset) noexcept;
[[nodiscard]] Result<uint32_t> Read32(Address address, uint16_t offset) noexcept;
[[nodiscard]] Status Write16(Address address, uint16_t offset, uint16_t value) noexcept;
[[nodiscard]] Status Write32(Address address, uint16_t offset, uint32_t value) noexcept;

[[nodiscard]] Result<Info> Probe(Address address) noexcept;
[[nodiscard]] Result<MsiXInfo> ReadMsiX(Address address) noexcept;
[[nodiscard]] Result<uint8_t> LegacyIrq(Address address) noexcept;
[[nodiscard]] Result<Info> FindByVendorDevice(uint16_t vendor_id, uint16_t device_id) noexcept;

using MatchFunction = bool (*)(const Info& info);
[[nodiscard]] Result<Info> FindFirst(MatchFunction match) noexcept;

// Returns the match at a zero-based index in PCI scan order.
[[nodiscard]] Result<Info> FindNth(MatchFunction match,
                                   uint32_t index) noexcept;

// Enables memory decoding and bus mastering for a device.
[[nodiscard]] Status EnableDevice(Address address) noexcept;

// Returns the assigned base address of a memory BAR, combining the pair
// when the BAR is 64 bit.
[[nodiscard]] Result<uint64_t> BarBase(const Info& info, uint8_t bar_index) noexcept;

}  // namespace og::drivers::pci

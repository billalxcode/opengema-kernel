#include <cstdint>
#include <cstdio>
#include <cstring>

#include "og/acpi/acpi.h"

namespace {

constexpr uint32_t kRsdpOffset = 0;
constexpr uint32_t kXsdtOffset = 64;
constexpr uint32_t kMadtOffset = 256;
constexpr uint32_t kSpcrOffset = 1024;
constexpr uint32_t kMcfgOffset = 1536;
constexpr uint32_t kFadtOffset = 2048;
constexpr uint32_t kBufferSize = 4096;
constexpr uint32_t kSdtHeaderLength = 36;

uint8_t g_buffer[kBufferSize]{};

void Write8(uint32_t offset, uint8_t value) {
  g_buffer[offset] = value;
}

void Write16(uint32_t offset, uint16_t value) {
  std::memcpy(g_buffer + offset, &value, sizeof(value));
}

void Write32(uint32_t offset, uint32_t value) {
  std::memcpy(g_buffer + offset, &value, sizeof(value));
}

void Write64(uint32_t offset, uint64_t value) {
  std::memcpy(g_buffer + offset, &value, sizeof(value));
}

void WriteHeader(uint32_t offset, const char* signature, uint32_t length,
                 uint8_t revision) {
  std::memcpy(g_buffer + offset, signature, 4);
  Write32(offset + 4, length);
  Write8(offset + 8, revision);
  Write8(offset + 9, 0);
}

void FixSdtChecksum(uint32_t offset, uint32_t length) {
  Write8(offset + 9, 0);
  uint8_t sum = 0;
  for (uint32_t index = 0; index < length; ++index) {
    sum = static_cast<uint8_t>(sum + g_buffer[offset + index]);
  }
  Write8(offset + 9, static_cast<uint8_t>(0U - sum));
}

void BuildMadt() {
  constexpr uint32_t kMadtHeaderLength = 44;
  constexpr uint32_t kGiccLength = 80;
  constexpr uint32_t kGicdLength = 24;
  constexpr uint32_t kLength =
      kMadtHeaderLength + (kGiccLength * 2U) + kGicdLength;
  WriteHeader(kMadtOffset, "APIC", kLength, 5);
  Write32(kMadtOffset + 36, 0);
  Write32(kMadtOffset + 40, 0);

  const uint32_t first = kMadtOffset + kMadtHeaderLength;
  Write8(first, 0x0B);
  Write8(first + 1, kGiccLength);
  Write32(first + 4, 0);
  Write32(first + 8, 0);
  Write32(first + 12, 1);
  Write64(first + 32, 0x08010000ULL);
  Write64(first + 60, 0x080A0000ULL);

  const uint32_t second = first + kGiccLength;
  Write8(second, 0x0B);
  Write8(second + 1, kGiccLength);
  Write32(second + 4, 1);
  Write32(second + 8, 1);
  Write32(second + 12, 1);
  Write64(second + 32, 0x08010000ULL);
  Write64(second + 60, 0x080C0000ULL);

  const uint32_t gicd = second + kGiccLength;
  Write8(gicd, 0x0C);
  Write8(gicd + 1, kGicdLength);
  Write32(gicd + 4, 0);
  Write64(gicd + 8, 0x08000000ULL);
  Write8(gicd + 20, 3);
  FixSdtChecksum(kMadtOffset, kLength);
}

void BuildSpcr() {
  constexpr uint32_t kLength = 80;
  WriteHeader(kSpcrOffset, "SPCR", kLength, 2);
  Write8(kSpcrOffset + 36, 3);
  Write8(kSpcrOffset + 40, 0);
  Write8(kSpcrOffset + 41, 32);
  Write8(kSpcrOffset + 42, 0);
  Write8(kSpcrOffset + 43, 3);
  Write64(kSpcrOffset + 44, 0x09000000ULL);
  FixSdtChecksum(kSpcrOffset, kLength);
}

void BuildMcfg() {
  constexpr uint32_t kLength = 60;
  WriteHeader(kMcfgOffset, "MCFG", kLength, 1);
  const uint32_t entry = kMcfgOffset + 44;
  Write64(entry, 0x4010000000ULL);
  Write16(entry + 8, 0);
  Write8(entry + 10, 0);
  Write8(entry + 11, 255);
  FixSdtChecksum(kMcfgOffset, kLength);
}

void BuildFadt() {
  constexpr uint32_t kLength = 276;
  WriteHeader(kFadtOffset, "FACP", kLength, 6);
  constexpr uint32_t kHardwareReduced = 1U << 20;
  constexpr uint32_t kResetRegister = 1U << 10;
  Write32(kFadtOffset + 112, kHardwareReduced | kResetRegister);
  Write8(kFadtOffset + 116, 0);
  Write8(kFadtOffset + 117, 8);
  Write8(kFadtOffset + 118, 0);
  Write8(kFadtOffset + 119, 1);
  Write64(kFadtOffset + 120, 0x09020000ULL);
  Write8(kFadtOffset + 128, 0x0F);
  Write16(kFadtOffset + 129, 3);
  FixSdtChecksum(kFadtOffset, kLength);
}

void BuildXsdt() {
  constexpr uint32_t kLength = kSdtHeaderLength + (4U * 8U);
  WriteHeader(kXsdtOffset, "XSDT", kLength, 1);
  Write64(kXsdtOffset + kSdtHeaderLength,
          reinterpret_cast<uint64_t>(g_buffer + kMadtOffset));
  Write64(kXsdtOffset + kSdtHeaderLength + 8U,
          reinterpret_cast<uint64_t>(g_buffer + kSpcrOffset));
  Write64(kXsdtOffset + kSdtHeaderLength + 16U,
          reinterpret_cast<uint64_t>(g_buffer + kMcfgOffset));
  Write64(kXsdtOffset + kSdtHeaderLength + 24U,
          reinterpret_cast<uint64_t>(g_buffer + kFadtOffset));
  FixSdtChecksum(kXsdtOffset, kLength);
}

void BuildRsdp() {
  std::memset(g_buffer, 0, sizeof(g_buffer));
  std::memcpy(g_buffer + kRsdpOffset, "RSD PTR ", 8);
  Write8(kRsdpOffset + 15, 2);
  Write32(kRsdpOffset + 16, 0);
  Write32(kRsdpOffset + 20, 36);
  Write64(kRsdpOffset + 24,
          reinterpret_cast<uint64_t>(g_buffer + kXsdtOffset));
  Write8(kRsdpOffset + 8, 0);
  Write8(kRsdpOffset + 32, 0);
  uint8_t sum = 0;
  for (uint32_t index = 0; index < 20; ++index) {
    sum = static_cast<uint8_t>(sum + g_buffer[index]);
  }
  Write8(kRsdpOffset + 8, static_cast<uint8_t>(0U - sum));
  sum = 0;
  for (uint32_t index = 0; index < 36; ++index) {
    sum = static_cast<uint8_t>(sum + g_buffer[index]);
  }
  Write8(kRsdpOffset + 32, static_cast<uint8_t>(0U - sum));
}

int Fail(const char* message) {
  std::printf("acpi probe: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  BuildRsdp();
  BuildXsdt();
  BuildMadt();
  BuildSpcr();
  BuildMcfg();
  BuildFadt();

  const uint64_t rsdp = reinterpret_cast<uint64_t>(g_buffer + kRsdpOffset);
  if (!og::IsOk(og::acpi::Init(rsdp)) || !og::acpi::Ready()) {
    return Fail("init");
  }

  const og::acpi::Info& info = og::acpi::GetInfo();
  if (info.revision != 2U || info.table_count != 4U) {
    return Fail("table count");
  }
  if (info.cpu_count != 2U) {
    return Fail("cpu count");
  }
  if (info.gic_distributor != 0x08000000ULL ||
      info.gic_redistributor != 0x080A0000ULL) {
    return Fail("gic addresses");
  }
  if (info.uart_base != 0x09000000ULL) {
    return Fail("uart address");
  }
  if (info.pci_ecam != 0x4010000000ULL ||
      info.pci_ecam_size != 0x10000000ULL) {
    return Fail("pci ecam");
  }
  if (info.has_gtdt) {
    return Fail("unexpected gtdt");
  }
  if (!info.hardware_reduced || !info.psci_compliant ||
      !info.psci_use_hvc) {
    return Fail("fadt arm flags");
  }
  if (!info.reset_register_supported ||
      info.reset_register_address != 0x09020000ULL ||
      info.reset_value != 0x0FU) {
    return Fail("fadt reset register");
  }

  const og::Result<og::acpi::Table> madt = og::acpi::FindTable("APIC");
  if (madt.is_err() || madt.value().length == 0U) {
    return Fail("find madt");
  }
  if (og::acpi::TableAt(4).is_ok()) {
    return Fail("table bounds");
  }
  if (og::acpi::FindTable("NOPE").is_ok()) {
    return Fail("missing table accepted");
  }

  g_buffer[kRsdpOffset + 8] ^= 0xFFU;
  if (og::IsOk(og::acpi::Init(rsdp)) || og::acpi::Ready()) {
    return Fail("bad checksum accepted");
  }

  std::printf("acpi probe: OK\n");
  return 0;
}

#pragma once

// Minimal ACPI table discovery for ARM64 platforms. Reads the RSDP passed
// by the firmware, walks XSDT or RSDT, validates every table checksum, and
// extracts the MADT, SPCR, and MCFG data the kernel can use.

#include <cstdint>

#include "og/core/status.h"

namespace og::acpi {

constexpr uint32_t kMaxTables = 32;
constexpr uint32_t kSignatureLength = 4;

struct Table {
  char signature[kSignatureLength + 1];
  uint32_t length;
  uint8_t revision;
  uint64_t address;
};

struct Info {
  bool ready;
  uint8_t revision;
  uint64_t rsdp_address;
  uint64_t root_table_address;
  uint32_t table_count;
  uint32_t cpu_count;
  uint64_t gic_distributor;
  uint64_t gic_redistributor;
  uint64_t uart_base;       // SPCR console UART.
  uint64_t pci_ecam;        // First MCFG allocation.
  uint64_t pci_ecam_size;
  bool has_gtdt;
  uint32_t gtdt_ns_timer_irq;
  uint32_t gtdt_ns_timer_flags;
  uint32_t gtdt_virtual_timer_irq;
  uint32_t gtdt_virtual_timer_flags;
  uint32_t gtdt_platform_timers;
  uint32_t fadt_flags;
  uint16_t arm_boot_flags;
  bool hardware_reduced;
  bool psci_compliant;
  bool psci_use_hvc;
  bool reset_register_supported;
  uint8_t reset_register_space;
  uint8_t reset_register_bit_width;
  uint8_t reset_register_bit_offset;
  uint8_t reset_register_access_size;
  uint64_t reset_register_address;
  uint8_t reset_value;
};

// Safe with a zero address: returns kNotFound and leaves the module unready.
[[nodiscard]] Status Init(uint64_t rsdp_address) noexcept;
[[nodiscard]] bool Ready() noexcept;
[[nodiscard]] const Info& GetInfo() noexcept;
[[nodiscard]] Result<Table> TableAt(uint32_t index) noexcept;
[[nodiscard]] Result<Table> FindTable(const char* signature) noexcept;

}  // namespace og::acpi

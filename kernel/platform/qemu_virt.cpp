#include "og/platform/platform.h"

#include "og/acpi/acpi.h"
#include "og/core/bootinfo.h"
#include "og/platform/fdt.h"
#include "og/platform/memory_map.h"

namespace og::platform {

namespace {

Platform g_platform{
  "qemu-virt",
  {memmap::kUart0Base, memmap::kUart0Size},
  {memmap::kGicDistBase, memmap::kGicDistSize},
  {memmap::kGicRedistBase, memmap::kGicRedistSize},
  {memmap::kRtcBase, memmap::kRtcSize},
  {memmap::kPciEcamBase, memmap::kPciEcamSize},
  memmap::kRamBase,
  24000000,
  2,
  kPsciConduitHvc,
};

}  // namespace

Status PlatformInit(const BootInfo& info) noexcept {
  bool have_tables = false;

  if (info.dtb_address != 0 && info.dtb_size != 0) {
    const Status status = fdt::Init(info.dtb_address, info.dtb_size);
    if (IsErr(status)) {
      if (!acpi::Ready()) {
        return status;
      }
    } else {
      const Result<uint32_t> psci = fdt::FindNode("/psci");
      if (psci.is_ok()) {
        const Result<std::span<const uint8_t>> method =
          fdt::FindProperty(psci.value(), "method");
        if (method.is_ok() && method.value().size() >= 1U) {
          const char first = static_cast<char>(method.value()[0]);
          if (first == 'h') {
            g_platform.psci_conduit = kPsciConduitHvc;
          } else if (first == 's') {
            g_platform.psci_conduit = kPsciConduitSmc;
          }
        }
      }

      const Result<uint32_t> cpus = fdt::FindNode("/cpus");
      if (cpus.is_ok()) {
        const uint32_t children = fdt::ChildCount(cpus.value());
        uint32_t count = 0;
        for (uint32_t index = 0; index < children; ++index) {
          const Result<uint32_t> child = fdt::Child(cpus.value(), index);
          if (child.is_err()) {
            break;
          }
          const Result<const char*> name = fdt::NodeName(child.value());
          if (name.is_err() || name.value() == nullptr) {
            continue;
          }
          if (name.value()[0] == 'c' && name.value()[1] == 'p' &&
              name.value()[2] == 'u' && name.value()[3] == '@') {
            ++count;
          }
        }
        if (count > 0U && count <= 64U) {
          g_platform.cpu_count = count;
        }
      }
      have_tables = true;
    }
  }

  if (acpi::Ready()) {
    const acpi::Info& acpi = acpi::GetInfo();
    if (acpi.cpu_count > 0U && acpi.cpu_count <= 64U) {
      g_platform.cpu_count = acpi.cpu_count;
    }
    if (acpi.gic_distributor != 0U) {
      g_platform.gic_distributor.base = acpi.gic_distributor;
    }
    if (acpi.gic_redistributor != 0U) {
      g_platform.gic_redistributor.base = acpi.gic_redistributor;
    }
    if (acpi.uart_base != 0U) {
      g_platform.uart0.base = acpi.uart_base;
    }
    if (acpi.pci_ecam != 0U && acpi.pci_ecam_size != 0U) {
      g_platform.pci_ecam.base = acpi.pci_ecam;
      g_platform.pci_ecam.size = acpi.pci_ecam_size;
    }
    have_tables = true;
  }

  return have_tables ? Status::kOk : Status::kNotFound;
}

const Platform& Current() noexcept {
  return g_platform;
}

}  // namespace og::platform

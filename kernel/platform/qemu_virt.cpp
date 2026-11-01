#include "og/platform/platform.h"

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
  if (info.dtb_address == 0 || info.dtb_size == 0) {
    return Status::kNotFound;
  }
  const Status status = fdt::Init(info.dtb_address, info.dtb_size);
  if (IsErr(status)) {
    return status;
  }

  const Result<uint32_t> psci = fdt::FindNode("/psci");
  if (psci.is_ok()) {
    const Result<std::span<const uint8_t>> method = fdt::FindProperty(psci.value(), "method");
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
      if (name.value()[0] == 'c' && name.value()[1] == 'p' && name.value()[2] == 'u' &&
          name.value()[3] == '@') {
        ++count;
      }
    }
    if (count > 0U && count <= 64U) {
      g_platform.cpu_count = count;
    }
  }

  return Status::kOk;
}

const Platform& Current() noexcept {
  return g_platform;
}

}  // namespace og::platform

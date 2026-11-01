#include "og/platform.h"

#include "og/memory_map.h"

namespace og::platform {

namespace {

constexpr Platform kQemuVirt{
  "qemu-virt",
  {memmap::kUart0Base, memmap::kUart0Size},
  {memmap::kGicDistBase, memmap::kGicDistSize},
  {memmap::kGicRedistBase, memmap::kGicRedistSize},
  {memmap::kPciEcamBase, memmap::kPciEcamSize},
  memmap::kRamBase,
  2,
  kPsciConduitHvc,
};

}  // namespace

const Platform& Current() noexcept {
  return kQemuVirt;
}

}  // namespace og::platform

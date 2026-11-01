#include "og/platform.h"

#include "og/memory_map.h"

namespace og::platform {

namespace {

constexpr Platform kQemuVirt{
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

const Platform& Current() noexcept {
  return kQemuVirt;
}

}  // namespace og::platform

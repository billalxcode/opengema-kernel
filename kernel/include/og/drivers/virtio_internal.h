#pragma once

// Private virtqueue layouts shared by the virtio queue implementation.
// Not part of the driver-facing API.

#include <cstdint>

namespace og::drivers::virtio::detail {

constexpr uint16_t kDescriptorNext = 1U << 0U;
constexpr uint16_t kDescriptorWrite = 1U << 1U;

struct Descriptor {
  uint64_t address;
  uint32_t length;
  uint16_t flags;
  uint16_t next;
};

static_assert(sizeof(Descriptor) == 16);

struct AvailableRing {
  uint16_t flags;
  uint16_t index;
};

struct UsedElement {
  uint32_t id;
  uint32_t length;
};

struct UsedRing {
  uint16_t flags;
  uint16_t index;
};

[[nodiscard]] inline uint64_t AlignUp(uint64_t value,
                                      uint64_t alignment) noexcept {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

}  // namespace og::drivers::virtio::detail

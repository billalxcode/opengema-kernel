#pragma once

// Canonical owner of bit manipulation for the kernel.
// All helpers are constexpr and take the type explicitly, so there is no
// second variant hidden in a driver.

#include <cstddef>
#include <cstdint>

namespace og::bits {

template <typename T>
[[nodiscard]] constexpr T Mask(unsigned width) noexcept {
  if (width == 0U) {
    return static_cast<T>(0);
  }
  if (width >= sizeof(T) * 8U) {
    return static_cast<T>(~static_cast<T>(0));
  }
  return static_cast<T>((static_cast<T>(1) << width) - static_cast<T>(1));
}

template <unsigned kPos, unsigned kWidth, typename T>
[[nodiscard]] constexpr T Extract(T value) noexcept {
  static_assert(kWidth > 0U, "field width must be positive");
  static_assert(kPos + kWidth <= sizeof(T) * 8U, "field exceeds type width");
  return static_cast<T>((value >> kPos) & Mask<T>(kWidth));
}

template <unsigned kPos, unsigned kWidth, typename T>
[[nodiscard]] constexpr T Insert(T reg, T value) noexcept {
  static_assert(kWidth > 0U, "field width must be positive");
  static_assert(kPos + kWidth <= sizeof(T) * 8U, "field exceeds type width");
  const T field_mask = static_cast<T>(Mask<T>(kWidth) << kPos);
  const T shifted = static_cast<T>((value << kPos) & field_mask);
  return static_cast<T>((reg & static_cast<T>(~field_mask)) | shifted);
}

template <typename T>
[[nodiscard]] constexpr T Bit(unsigned index) noexcept {
  if (index >= sizeof(T) * 8U) {
    return static_cast<T>(0);
  }
  return static_cast<T>(static_cast<T>(1) << index);
}

// Alignment helpers assume a power of two, which is the only case the
// kernel uses for pages, stacks, and DMA buffers.
template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, T align) noexcept {
  if (align == static_cast<T>(0)) {
    return value;
  }
  return static_cast<T>((value + align - static_cast<T>(1)) & static_cast<T>(~(align - static_cast<T>(1))));
}

template <typename T>
[[nodiscard]] constexpr T AlignDown(T value, T align) noexcept {
  if (align == static_cast<T>(0)) {
    return value;
  }
  return static_cast<T>(value & static_cast<T>(~(align - static_cast<T>(1))));
}

template <typename T>
[[nodiscard]] constexpr bool IsAligned(T value, T align) noexcept {
  if (align == static_cast<T>(0)) {
    return false;
  }
  return (value & static_cast<T>(align - static_cast<T>(1))) == static_cast<T>(0);
}

}  // namespace og::bits

#pragma once

// Private FDT tokenizer helpers shared by the public FDT units.
// Not part of the platform API.

#include <cstdint>

#include "og/core/status.h"

namespace og::platform::fdt::detail {

constexpr uint32_t kTokenBeginNode = 1;
constexpr uint32_t kTokenEndNode = 2;
constexpr uint32_t kTokenProperty = 3;
constexpr uint32_t kTokenNop = 4;
constexpr uint32_t kTokenEnd = 9;

[[nodiscard]] uint32_t ReadU32(uint32_t offset) noexcept;

[[nodiscard]] uint32_t Align4(uint32_t value) noexcept;

[[nodiscard]] bool Valid(uint32_t offset, uint32_t length) noexcept;

[[nodiscard]] const char* StructString(uint32_t offset) noexcept;

[[nodiscard]] const char* StringsEntry(uint32_t name_offset) noexcept;

[[nodiscard]] Result<uint32_t> NodeBodyOffset(uint32_t node) noexcept;

[[nodiscard]] Result<uint32_t> SkipNode(uint32_t offset) noexcept;

[[nodiscard]] uint32_t StructStart() noexcept;

[[nodiscard]] uint32_t StructEnd() noexcept;

[[nodiscard]] const uint8_t* Base() noexcept;

}  // namespace og::platform::fdt::detail

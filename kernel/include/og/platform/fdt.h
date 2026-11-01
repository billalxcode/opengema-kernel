#pragma once

// Minimal flattened device tree reader. Only what the platform layer needs:
// find a node by path, read a property, and count children.

#include <cstdint>
#include <span>

#include "og/core/status.h"

namespace og::platform::fdt {

[[nodiscard]] Status Init(uint64_t address, uint64_t size) noexcept;
[[nodiscard]] bool Ready() noexcept;

// Node handle is the offset of its BEGIN_NODE token inside the structure
// block.
[[nodiscard]] Result<uint32_t> FindNode(const char* path) noexcept;
[[nodiscard]] Result<std::span<const uint8_t>> FindProperty(uint32_t node,
                                                            const char* name) noexcept;
[[nodiscard]] uint32_t ChildCount(uint32_t node) noexcept;
[[nodiscard]] Result<uint32_t> Child(uint32_t node, uint32_t index) noexcept;
[[nodiscard]] Result<const char*> NodeName(uint32_t node) noexcept;

}  // namespace og::platform::fdt

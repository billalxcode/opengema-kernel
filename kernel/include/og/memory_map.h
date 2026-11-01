#pragma once

#include <cstdint>

namespace og {

namespace memmap {

constexpr uint64_t kRamBase = 0x40000000ULL;
constexpr uint64_t kUart0Base = 0x09000000ULL;
constexpr uint64_t kUart0Size = 0x1000ULL;
constexpr uint64_t kGicDistBase = 0x08000000ULL;
constexpr uint64_t kPageSize = 4096ULL;

}  // namespace memmap

}  // namespace og

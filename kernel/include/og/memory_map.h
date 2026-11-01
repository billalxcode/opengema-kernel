#pragma once

#include <cstdint>

namespace og {

namespace memmap {

constexpr uint64_t kRamBase = 0x40000000ULL;
constexpr uint64_t kUart0Base = 0x09000000ULL;
constexpr uint64_t kUart0Size = 0x1000ULL;
constexpr uint64_t kGicDistBase = 0x08000000ULL;
constexpr uint64_t kGicDistSize = 0x10000ULL;
constexpr uint64_t kGicRedistBase = 0x080A0000ULL;
constexpr uint64_t kGicRedistSize = 0x20000ULL;
constexpr uint64_t kRtcBase = 0x09010000ULL;
constexpr uint64_t kRtcSize = 0x1000ULL;
constexpr uint64_t kPciEcamBase = 0x4010000000ULL;
constexpr uint64_t kPciEcamSize = 0x10000000ULL;
constexpr uint64_t kPageSize = 4096ULL;

}  // namespace memmap

}  // namespace og

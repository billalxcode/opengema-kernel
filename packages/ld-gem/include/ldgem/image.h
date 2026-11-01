#pragma once

// Loads an ET_DYN shared object into a fresh executable mapping.

#include "ldgem/elf.h"
#include "ldgem/types.h"

namespace ldgem {

constexpr u32 kMaxSegments = 8;

struct Segment {
  u64 virtual_address;
  u64 file_offset;
  u64 file_size;
  u64 memory_size;
  bool executable;
};

struct Image {
  u16 type;
  u64 entry;
  u64 min_address;
  u64 max_address;
  u64 dynamic_address;
  u32 segment_count;
  Segment segments[kMaxSegments];
};

// Validates the header and program headers. data holds the start of the
// file and must cover the program header table.
[[nodiscard]] bool ParseImage(const u8* data, u64 size, u64 file_size,
                              Image& out) noexcept;

// Maps path and copies every PT_LOAD segment. base receives the load bias,
// dynamic receives the biased address of PT_DYNAMIC.
[[nodiscard]] bool LoadImage(const char* path, Image& image, u64& base,
                             u64& dynamic) noexcept;

}  // namespace ldgem

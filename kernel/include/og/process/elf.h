#pragma once

// Validates a static or position independent ELF64 executable before the
// process loader maps it. ET_EXEC and ET_DYN AArch64 images are accepted.
// Dynamic images are detected via PT_INTERP.

#include <cstdint>
#include <span>

#include "og/core/status.h"

namespace og::process::elf {

// Leave room for the stack region. The address space tracks a limited
// number of user regions.
constexpr uint32_t kMaxSegments = 6;
constexpr uint32_t kMaxProgramHeaders = 16;
constexpr uint64_t kPageSize = 4096;
constexpr uint32_t kHeaderBufferSize = 1024;

enum class ImageType : uint32_t {
  kUnknown = 0,
  kExecutable = 2,
  kDynamic = 3,
};

struct Segment {
  uint64_t virtual_address;
  uint64_t file_offset;
  uint64_t file_size;
  uint64_t memory_size;
  bool executable;
};

struct Image {
  ImageType type;
  uint64_t entry;
  uint64_t dynamic_address;
  uint64_t min_address;
  uint64_t max_address;
  uint32_t segment_count;
  Segment segments[kMaxSegments];
  uint64_t interpreter_offset;
  uint64_t interpreter_size;
};

// header holds the start of the file. It must cover the ELF header and the
// program header table. file_size is the full size of the file on disk.
[[nodiscard]] Result<Image> Parse(std::span<const uint8_t> header,
                                  uint64_t file_size) noexcept;

}  // namespace og::process::elf

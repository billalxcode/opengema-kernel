#include "og/process/elf.h"

#include "bit.h"
#include "og/mem/vmm.h"

namespace og::process::elf {

namespace {

constexpr uint64_t kElfHeaderSize = 64;
constexpr uint64_t kProgramHeaderSize = 56;
constexpr uint32_t kElfClass64 = 2;
constexpr uint32_t kElfDataLittleEndian = 1;
constexpr uint32_t kElfVersionCurrent = 1;
constexpr uint16_t kTypeExecutable = 2;
constexpr uint16_t kMachineAarch64 = 183;
constexpr uint32_t kProgramTypeLoad = 1;
constexpr uint32_t kProgramTypeInterp = 3;
constexpr uint32_t kProgramFlagExecute = 1;
constexpr uint64_t kPageMask = kPageSize - 1U;

[[nodiscard]] uint16_t Read16(std::span<const uint8_t> bytes,
                              uint64_t offset) noexcept {
  return static_cast<uint16_t>(bytes[offset]) |
         static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] uint32_t Read32(std::span<const uint8_t> bytes,
                              uint64_t offset) noexcept {
  uint32_t value = 0;
  for (uint32_t index = 0; index < 4U; ++index) {
    value |= static_cast<uint32_t>(bytes[offset + index]) << (index * 8U);
  }
  return value;
}

[[nodiscard]] uint64_t Read64(std::span<const uint8_t> bytes,
                              uint64_t offset) noexcept {
  uint64_t value = 0;
  for (uint32_t index = 0; index < 8U; ++index) {
    value |= static_cast<uint64_t>(bytes[offset + index]) << (index * 8U);
  }
  return value;
}

[[nodiscard]] bool RangeInside(uint64_t offset, uint64_t size,
                               uint64_t limit) noexcept {
  if (offset > limit) {
    return false;
  }
  return size <= (limit - offset);
}

}  // namespace

Result<Image> Parse(std::span<const uint8_t> header, uint64_t file_size) noexcept {
  if (header.size() < kElfHeaderSize) {
    return Result<Image>::Err(Status::kInvalidArg);
  }
  if (file_size < kElfHeaderSize) {
    return Result<Image>::Err(Status::kInvalidArg);
  }
  if (header[0] != 0x7FU || header[1] != 'E' || header[2] != 'L' ||
      header[3] != 'F') {
    return Result<Image>::Err(Status::kInvalidArg);
  }
  if (header[4] != kElfClass64 || header[5] != kElfDataLittleEndian ||
      header[6] != kElfVersionCurrent) {
    return Result<Image>::Err(Status::kInvalidArg);
  }
  if (Read16(header, 16) != kTypeExecutable ||
      Read16(header, 18) != kMachineAarch64 ||
      Read32(header, 20) != kElfVersionCurrent) {
    return Result<Image>::Err(Status::kUnsupported);
  }

  const uint64_t entry = Read64(header, 24);
  const uint64_t program_offset = Read64(header, 32);
  const uint16_t header_size = Read16(header, 52);
  const uint16_t program_entry_size = Read16(header, 54);
  const uint16_t program_count = Read16(header, 56);

  if (header_size != kElfHeaderSize ||
      program_entry_size != kProgramHeaderSize || program_count == 0U ||
      program_count > kMaxProgramHeaders) {
    return Result<Image>::Err(Status::kInvalidArg);
  }
  if (!RangeInside(program_offset,
                   static_cast<uint64_t>(program_count) * kProgramHeaderSize,
                   header.size())) {
    return Result<Image>::Err(Status::kInvalidArg);
  }
  if (entry < mem::kUserSpaceBase ||
      entry >= (mem::kUserSpaceBase + mem::kUserSpaceSize)) {
    return Result<Image>::Err(Status::kInvalidArg);
  }

  Image image{};
  image.entry = entry;

  for (uint32_t index = 0; index < program_count; ++index) {
    const uint64_t base =
      program_offset + (static_cast<uint64_t>(index) * kProgramHeaderSize);
    const uint32_t type = Read32(header, base);
    const uint32_t flags = Read32(header, base + 4U);
    const uint64_t offset = Read64(header, base + 8U);
    const uint64_t virtual_address = Read64(header, base + 16U);
    const uint64_t file_bytes = Read64(header, base + 32U);
    const uint64_t memory_bytes = Read64(header, base + 40U);

    if (type == kProgramTypeInterp) {
      if (file_bytes == 0U || !RangeInside(offset, file_bytes, file_size)) {
        return Result<Image>::Err(Status::kInvalidArg);
      }
      image.interpreter_offset = offset;
      image.interpreter_size = file_bytes;
      continue;
    }
    if (type != kProgramTypeLoad) {
      continue;
    }
    if (image.segment_count >= kMaxSegments) {
      return Result<Image>::Err(Status::kInvalidArg);
    }
    if (file_bytes > memory_bytes || memory_bytes == 0U) {
      return Result<Image>::Err(Status::kInvalidArg);
    }
    if (!RangeInside(offset, file_bytes, file_size)) {
      return Result<Image>::Err(Status::kInvalidArg);
    }
    if (virtual_address < mem::kUserSpaceBase ||
        !RangeInside(virtual_address, memory_bytes,
                     mem::kUserSpaceBase + mem::kUserSpaceSize)) {
      return Result<Image>::Err(Status::kInvalidArg);
    }
    const uint64_t page_offset = virtual_address & kPageMask;
    if ((offset & kPageMask) != page_offset || offset < page_offset) {
      return Result<Image>::Err(Status::kInvalidArg);
    }

    Segment& segment = image.segments[image.segment_count];
    segment.virtual_address = virtual_address;
    segment.file_offset = offset;
    segment.file_size = file_bytes;
    segment.memory_size = memory_bytes;
    segment.executable = (flags & kProgramFlagExecute) != 0U;
    ++image.segment_count;
  }

  if (image.segment_count == 0U) {
    return Result<Image>::Err(Status::kInvalidArg);
  }
  return Result<Image>::Ok(image);
}

}  // namespace og::process::elf

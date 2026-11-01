#include "og/process/elf.h"

#include "bit.h"
#include "endian.h"
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
constexpr uint32_t kProgramTypeDynamic = 2;
constexpr uint32_t kProgramTypeInterp = 3;
constexpr uint32_t kProgramFlagExecute = 1;
constexpr uint64_t kPageMask = kPageSize - 1U;

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
  if (endian::ReadLe16(header.data() + 16) != kTypeExecutable ||
      endian::ReadLe16(header.data() + 18) != kMachineAarch64 ||
      endian::ReadLe32(header.data() + 20) != kElfVersionCurrent) {
    return Result<Image>::Err(Status::kUnsupported);
  }

  const uint64_t entry = endian::ReadLe64(header.data() + 24);
  const uint64_t program_offset = endian::ReadLe64(header.data() + 32);
  const uint16_t header_size = endian::ReadLe16(header.data() + 52);
  const uint16_t program_entry_size = endian::ReadLe16(header.data() + 54);
  const uint16_t program_count = endian::ReadLe16(header.data() + 56);

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
    const uint32_t type = endian::ReadLe32(header.data() + base);
    const uint32_t flags = endian::ReadLe32(header.data() + base + 4U);
    const uint64_t offset = endian::ReadLe64(header.data() + base + 8U);
    const uint64_t virtual_address = endian::ReadLe64(header.data() + base + 16U);
    const uint64_t file_bytes = endian::ReadLe64(header.data() + base + 32U);
    const uint64_t memory_bytes = endian::ReadLe64(header.data() + base + 40U);

    if (type == kProgramTypeInterp) {
      if (file_bytes == 0U) {
        continue;
      }
      if (!RangeInside(offset, file_bytes, file_size)) {
        return Result<Image>::Err(Status::kInvalidArg);
      }
      image.interpreter_offset = offset;
      image.interpreter_size = file_bytes;
      continue;
    }
    if (type == kProgramTypeDynamic) {
      if (file_bytes == 0U) {
        continue;
      }
      if (virtual_address < mem::kUserSpaceBase ||
          virtual_address >= (mem::kUserSpaceBase + mem::kUserSpaceSize)) {
        return Result<Image>::Err(Status::kInvalidArg);
      }
      image.dynamic_address = virtual_address;
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
  if (image.dynamic_address != 0U && image.interpreter_size == 0U) {
    return Result<Image>::Err(Status::kUnsupported);
  }
  return Result<Image>::Ok(image);
}

}  // namespace og::process::elf

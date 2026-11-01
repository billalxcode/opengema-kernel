#include "ldgem/image.h"

#include "kgema/file.h"
#include "kgema/memory.h"

namespace ldgem {

namespace {

constexpr u64 kHeaderBufferSize = 1024;
constexpr u64 kPageMask = kPageSize - 1U;

[[nodiscard]] bool ReadExact(kgema::i64 descriptor, void* buffer,
                             u64 size) noexcept {
  auto* destination = static_cast<u8*>(buffer);
  u64 done = 0;
  while (done < size) {
    const kgema::i64 count = kgema::read(descriptor, destination + done,
                                         size - done);
    if (count <= 0) {
      return false;
    }
    done += static_cast<u64>(count);
  }
  return true;
}

}  // namespace

bool ParseImage(const u8* data, u64 size, u64 file_size, Image& out) noexcept {
  if (data == nullptr || size < elf::kHeaderSize ||
      file_size < elf::kHeaderSize) {
    return false;
  }
  if (data[0] != 0x7FU || data[1] != 'E' || data[2] != 'L' || data[3] != 'F') {
    return false;
  }
  if (data[4] != 2U || data[5] != 1U || data[6] != 1U) {
    return false;
  }
  if (elf::Read16(data, 16) != elf::kTypeDyn ||
      elf::Read16(data, 18) != elf::kMachineAarch64 ||
      elf::Read32(data, 20) != 1U) {
    return false;
  }
  if (elf::Read16(data, 52) != elf::kHeaderSize ||
      elf::Read16(data, 54) != elf::kProgramHeaderSize) {
    return false;
  }

  const u16 program_count = elf::Read16(data, 56);
  const u64 program_offset = elf::Read64(data, 32);
  if (program_count == 0U || program_count > kMaxSegments ||
      program_offset > size ||
      static_cast<u64>(program_count) * elf::kProgramHeaderSize >
        (size - program_offset)) {
    return false;
  }

  Image image{};
  image.type = elf::kTypeDyn;
  image.min_address = ~0ULL;
  image.max_address = 0;

  for (u32 index = 0; index < program_count; ++index) {
    const u64 base =
      program_offset + (static_cast<u64>(index) * elf::kProgramHeaderSize);
    const u32 type = elf::Read32(data, base);
    const u64 offset = elf::Read64(data, base + 8U);
    const u64 virtual_address = elf::Read64(data, base + 16U);
    const u64 file_bytes = elf::Read64(data, base + 32U);
    const u64 memory_bytes = elf::Read64(data, base + 40U);

    if (type == elf::kProgramDynamic) {
      image.dynamic_address = virtual_address;
      continue;
    }
    if (type != elf::kProgramLoad) {
      continue;
    }
    if (image.segment_count >= kMaxSegments || memory_bytes == 0U ||
        file_bytes > memory_bytes || offset > file_size ||
        file_bytes > (file_size - offset) ||
        virtual_address > (~0ULL - memory_bytes)) {
      return false;
    }
    if ((virtual_address & kPageMask) != (offset & kPageMask)) {
      return false;
    }

    Segment& segment = image.segments[image.segment_count];
    segment.virtual_address = virtual_address;
    segment.file_offset = offset;
    segment.file_size = file_bytes;
    segment.memory_size = memory_bytes;
    segment.executable = (elf::Read32(data, base + 4U) & 1U) != 0U;
    ++image.segment_count;

    if (virtual_address < image.min_address) {
      image.min_address = virtual_address;
    }
    if (virtual_address + memory_bytes > image.max_address) {
      image.max_address = virtual_address + memory_bytes;
    }
  }

  if (image.segment_count == 0U || image.dynamic_address == 0U) {
    return false;
  }
  image.min_address &= ~kPageMask;
  image.max_address = elf::AlignUp(image.max_address, kPageSize);
  out = image;
  return true;
}

bool LoadImage(const char* path, Image& image, u64& base, u64& dynamic) noexcept {
  if (path == nullptr) {
    return false;
  }
  const kgema::i64 descriptor = kgema::open(path);
  if (descriptor < 0) {
    return false;
  }
  kgema::Stat info{};
  if (!kgema::stat(path, info) || info.size < elf::kHeaderSize) {
    (void)kgema::close(descriptor);
    return false;
  }

  u8 header[kHeaderBufferSize]{};
  const u64 header_size =
    info.size < sizeof(header) ? info.size : static_cast<u64>(sizeof(header));
  if (!ReadExact(descriptor, header, header_size)) {
    (void)kgema::close(descriptor);
    return false;
  }
  if (!ParseImage(header, header_size, info.size, image)) {
    (void)kgema::close(descriptor);
    return false;
  }

  const u64 span = image.max_address - image.min_address;
  void* mapping = kgema::mmap_executable(span);
  if (mapping == nullptr) {
    (void)kgema::close(descriptor);
    return false;
  }
  base = reinterpret_cast<u64>(mapping) - image.min_address;

  for (u32 index = 0; index < image.segment_count; ++index) {
    const Segment& segment = image.segments[index];
    if (segment.file_size == 0U) {
      continue;
    }
    if (!kgema::seek(descriptor, segment.file_offset)) {
      (void)kgema::close(descriptor);
      return false;
    }
    auto* destination = reinterpret_cast<u8*>(base + segment.virtual_address);
    if (!ReadExact(descriptor, destination, segment.file_size)) {
      (void)kgema::close(descriptor);
      return false;
    }
  }

  dynamic = base + image.dynamic_address;
  (void)kgema::close(descriptor);
  return true;
}

}  // namespace ldgem

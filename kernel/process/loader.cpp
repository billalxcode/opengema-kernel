#include "og/process/loader.h"

#include "bit.h"
#include "memory.h"
#include "og/mem/heap.h"

namespace og::process::loader {

Status ReadImage(fs::FileTable& files, uint32_t file, uint64_t file_size,
                 elf::Image& image) noexcept {
  uint8_t header[elf::kHeaderBufferSize]{};
  const uint64_t size =
    file_size < sizeof(header) ? file_size : static_cast<uint64_t>(sizeof(header));
  Status status = fs::FsSeek(files, file, 0);
  if (IsErr(status)) {
    return status;
  }
  uint64_t read = 0;
  status = fs::FsRead(files, file, std::span<uint8_t>(header, size), read);
  if (IsErr(status) || read != size) {
    return Status::kIoError;
  }
  const Result<elf::Image> parsed =
    elf::Parse(std::span<const uint8_t>(header, read), file_size);
  if (parsed.is_err()) {
    return parsed.status();
  }
  image = parsed.value();
  return Status::kOk;
}

Status MapImage(fs::FileTable& files, uint32_t file, const elf::Image& image,
                mem::AddressSpace& space, uint64_t base) noexcept {
  Status status = Status::kOk;
  for (uint32_t index = 0;
       index < image.segment_count && IsOk(status); ++index) {
    const elf::Segment& segment = image.segments[index];
    const uint64_t virtual_address = base + segment.virtual_address;
    const uint64_t page_offset = virtual_address & (elf::kPageSize - 1U);
    const uint64_t map_base = virtual_address - page_offset;
    const uint64_t map_size =
      bits::AlignUp(page_offset + segment.memory_size, elf::kPageSize);
    status = mem::VmmMapUser(space, map_base, map_size, segment.executable);
    if (IsErr(status)) {
      continue;
    }
    if (segment.file_size == 0U) {
      if (segment.executable) {
        status = mem::VmmProtectUser(space, map_base, map_size, true, false);
      }
      continue;
    }

    const Result<void*> buffer = mem::HeapAlloc(segment.file_size);
    if (buffer.is_err()) {
      status = buffer.status();
      break;
    }
    status = fs::FsSeek(files, file, segment.file_offset);
    uint64_t read = 0;
    if (IsOk(status)) {
      status = fs::FsRead(
        files, file,
        std::span<uint8_t>(static_cast<uint8_t*>(buffer.value()),
                           segment.file_size),
        read);
    }
    if (IsOk(status) && read != segment.file_size) {
      status = Status::kIoError;
    }
    if (IsOk(status)) {
      status = mem::VmmCopyOut(space, virtual_address, buffer.value(),
                               segment.file_size);
    }
    if (IsOk(status) && segment.executable) {
      status = mem::VmmProtectUser(space, map_base, map_size, true, false);
    }
    (void)mem::HeapFree(buffer.value());
  }
  return status;
}

Status ReadInterpreterPath(fs::FileTable& files, uint32_t file,
                           const elf::Image& image, char* path,
                           uint64_t capacity) noexcept {
  if (image.interpreter_size < 2U || image.interpreter_size > capacity) {
    return Status::kInvalidArg;
  }
  Status status = fs::FsSeek(files, file, image.interpreter_offset);
  if (IsErr(status)) {
    return status;
  }
  uint64_t read = 0;
  status = fs::FsRead(
    files, file,
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(path),
                       image.interpreter_size),
    read);
  if (IsErr(status) || read != image.interpreter_size) {
    return Status::kIoError;
  }
  if (path[image.interpreter_size - 1U] != '\0' || path[0] != '/') {
    return Status::kInvalidArg;
  }
  return Status::kOk;
}

}  // namespace og::process::loader

#pragma once

// ELF loading helpers: read and parse an image, then map its segments into
// a user address space. Process lifecycle stays in process.cpp.

#include <cstdint>

#include "og/core/status.h"
#include "og/fs/vfs.h"
#include "og/mem/vmm.h"
#include "og/process/elf.h"

namespace og::process::loader {

[[nodiscard]] Status ReadImage(fs::FileTable& files, uint32_t file,
                               uint64_t file_size, elf::Image& image) noexcept;

[[nodiscard]] Status MapImage(fs::FileTable& files, uint32_t file,
                              const elf::Image& image,
                              mem::AddressSpace& space,
                              uint64_t base) noexcept;

[[nodiscard]] Status ReadInterpreterPath(fs::FileTable& files, uint32_t file,
                                         const elf::Image& image, char* path,
                                         uint64_t capacity) noexcept;

}  // namespace og::process::loader

#include "ldgem/linker.h"

#include "kgema/io.h"
#include "gsl/layout.h"
#include "ldgem/elf.h"
#include "ldgem/image.h"
#include "ldgem/text.h"

namespace ldgem {

namespace {

constexpr u64 kMaxPath = 96;

void CopyName(char* destination, u64 capacity, const char* source) noexcept {
  u64 index = 0;
  if (source != nullptr) {
    while ((index + 1U) < capacity && source[index] != '\0') {
      destination[index] = source[index];
      ++index;
    }
  }
  destination[index] = '\0';
}

}  // namespace

bool Linker::AddMain(u64 dynamic, const char* name) noexcept {
  return AddObject(name, 0, dynamic, true);
}

bool Linker::AddObject(const char* name, u64 base, u64 dynamic,
                       bool is_main) noexcept {
  if (count_ >= kMaxObjects || name == nullptr || dynamic == 0U) {
    return false;
  }
  Object& object = objects_[count_];
  object = Object{};
  CopyName(object.name, kMaxName, name);
  object.base = base;
  object.dynamic = dynamic;
  object.is_main = is_main;
  if (!ParseDynamic(object)) {
    ReportSymbol("ld-gem: bad dynamic section: ", name);
    return false;
  }
  ++count_;
  return true;
}

bool Linker::HasObject(const char* name) const noexcept {
  for (u32 index = 0; index < count_; ++index) {
    if (elf::Same(objects_[index].name, name)) {
      return true;
    }
  }
  return false;
}

bool Linker::AddLibrary(const char* name) noexcept {
  if (name == nullptr || name[0] == '\0' || HasObject(name)) {
    return name != nullptr && name[0] != '\0';
  }
  const u64 name_length = TextLength(name);
  const u64 directory_length = TextLength(gsl::layout::kLibraries);
  if ((directory_length + 1U + name_length + 1U) > kMaxPath) {
    ReportSymbol("ld-gem: library name too long: ", name);
    return false;
  }
  char path[kMaxPath]{};
  for (u64 index = 0; index < directory_length; ++index) {
    path[index] = gsl::layout::kLibraries[index];
  }
  path[directory_length] = '/';
  for (u64 index = 0; index < name_length; ++index) {
    path[directory_length + 1U + index] = name[index];
  }
  path[directory_length + 1U + name_length] = '\0';

  Image image{};
  u64 base = 0;
  u64 dynamic = 0;
  if (!LoadImage(path, image, base, dynamic)) {
    ReportSymbol("ld-gem: cannot load: ", path);
    return false;
  }
  if (!AddObject(name, base, dynamic, false)) {
    return false;
  }
  return true;
}

u64 Linker::Lookup(const char* name) noexcept {
  if (name == nullptr) {
    return 0;
  }
  const u32 hash_value = elf::HashSymbolName(name);
  for (u32 object_index = 0; object_index < count_; ++object_index) {
    const Object& object = objects_[object_index];
    if (object.hash == 0U) {
      continue;
    }
    const auto* hash = reinterpret_cast<const u8*>(object.hash);
    const u32 bucket_count = elf::Read32(hash, 0);
    const u32 chain_count = elf::Read32(hash, 4);
    if (bucket_count == 0U || chain_count == 0U) {
      continue;
    }
    u32 symbol_index =
      elf::Read32(hash, 8U + (static_cast<u64>(hash_value % bucket_count) * 4U));
    for (u32 guard = 0; symbol_index != 0U && symbol_index < chain_count &&
                        guard < chain_count;
         ++guard) {
      const auto* symbol =
        reinterpret_cast<const u8*>(object.symbol_table) +
        (static_cast<u64>(symbol_index) * elf::kSymbolEntrySize);
      const u32 name_offset = elf::Read32(symbol, 0);
      const u32 info = symbol[4];
      const u16 section = elf::Read16(symbol, 6);
      const u32 bind = info >> 4U;
      if (section != elf::kSectionUndefined && bind != elf::kSymbolBindLocal &&
          elf::Same(
            reinterpret_cast<const char*>(object.string_table + name_offset),
            name)) {
        return object.base + elf::Read64(symbol, 8);
      }
      symbol_index =
        elf::Read32(hash, 8U + (static_cast<u64>(bucket_count) * 4U) +
                             (static_cast<u64>(symbol_index) * 4U));
    }
  }
  return 0;
}

}  // namespace ldgem

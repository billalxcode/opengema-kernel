#include "ldgem/linker.h"

#include "kgema/io.h"
#include "gsl/layout.h"
#include "ldgem/elf.h"
#include "ldgem/image.h"
#include "ldgem/text.h"

namespace ldgem {

namespace {

constexpr u64 kMaxPath = 96;
constexpr u32 kMaxDynamicEntries = 128;

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

bool Linker::ParseDynamic(Object& object) noexcept {
  const auto* entries = reinterpret_cast<const u8*>(object.dynamic);
  for (u32 index = 0; index < kMaxDynamicEntries; ++index) {
    const u8* entry = entries + (static_cast<u64>(index) * 16U);
    const i64 tag = static_cast<i64>(elf::Read64(entry, 0));
    const u64 value = elf::Read64(entry, 8);
    if (tag == elf::kTagNull) {
      break;
    }
    switch (tag) {
      case elf::kTagHash:
        object.hash = object.base + value;
        break;
      case elf::kTagStrtab:
        object.string_table = object.base + value;
        break;
      case elf::kTagSymtab:
        object.symbol_table = object.base + value;
        break;
      case elf::kTagRela:
        object.rela = object.base + value;
        break;
      case elf::kTagRelaSize:
        object.rela_size = value;
        break;
      case elf::kTagJmpRel:
        object.jump_rela = object.base + value;
        break;
      case elf::kTagPltRelSize:
        object.jump_rela_size = value;
        break;
      case elf::kTagInit:
        object.init = object.base + value;
        break;
      case elf::kTagInitArray:
        object.init_array = object.base + value;
        break;
      case elf::kTagInitArraySize:
        object.init_array_size = value;
        break;
      default:
        break;
    }
  }

  if (object.string_table == 0U || object.symbol_table == 0U) {
    return false;
  }
  if (!object.is_main && object.hash == 0U) {
    return false;
  }
  return true;
}

bool Linker::LoadLibraries() noexcept {
  for (u32 index = 0; index < count_; ++index) {
    if (!LoadNeeded(objects_[index])) {
      return false;
    }
  }
  return true;
}

bool Linker::LoadNeeded(const Object& object) noexcept {
  const auto* entries = reinterpret_cast<const u8*>(object.dynamic);
  for (u32 index = 0; index < kMaxDynamicEntries; ++index) {
    const u8* entry = entries + (static_cast<u64>(index) * 16U);
    const i64 tag = static_cast<i64>(elf::Read64(entry, 0));
    const u64 value = elf::Read64(entry, 8);
    if (tag == elf::kTagNull) {
      break;
    }
    if (tag != elf::kTagNeeded) {
      continue;
    }
    const auto* name =
      reinterpret_cast<const char*>(object.string_table + value);
    if (!AddLibrary(name)) {
      return false;
    }
  }
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

bool Linker::RelocateAll() noexcept {
  for (u32 index = 0; index < count_; ++index) {
    if (!Relocate(objects_[index])) {
      return false;
    }
  }
  return true;
}

bool Linker::Relocate(Object& object) noexcept {
  if (object.rela != 0U && object.rela_size != 0U) {
    if (!RelocateRange(object, object.rela, object.rela_size)) {
      return false;
    }
  }
  if (object.jump_rela != 0U && object.jump_rela_size != 0U) {
    if (!RelocateRange(object, object.jump_rela, object.jump_rela_size)) {
      return false;
    }
  }
  return true;
}

bool Linker::RelocateRange(Object& object, u64 address, u64 size) noexcept {
  const auto* table = reinterpret_cast<const u8*>(address);
  for (u64 offset = 0; (offset + elf::kRelaEntrySize) <= size;
       offset += elf::kRelaEntrySize) {
    const u64 relocation_offset = elf::Read64(table, offset);
    const u64 info = elf::Read64(table, offset + 8U);
    const i64 addend = static_cast<i64>(elf::Read64(table, offset + 16U));
    const u32 type = static_cast<u32>(info & 0xFFFFFFFFULL);
    const u32 symbol_index = static_cast<u32>(info >> 32U);
    auto* where = reinterpret_cast<u64*>(object.base + relocation_offset);

    if (type == elf::kRelocationNone) {
      continue;
    }
    if (type == elf::kRelocationRelative) {
      *where = object.base + static_cast<u64>(addend);
      continue;
    }
    if (type != elf::kRelocationAbs64 && type != elf::kRelocationGlobDat &&
        type != elf::kRelocationJumpSlot) {
      WriteText("ld-gem: unsupported relocation\n");
      return false;
    }

    const auto* symbol =
      reinterpret_cast<const u8*>(object.symbol_table) +
      (static_cast<u64>(symbol_index) * elf::kSymbolEntrySize);
    const auto* name = reinterpret_cast<const char*>(
      object.string_table + elf::Read32(symbol, 0));
    const u64 target = Lookup(name);
    if (target == 0U) {
      ReportSymbol("ld-gem: unresolved: ", name);
      return false;
    }
    *where = target + static_cast<u64>(addend);
  }
  return true;
}

bool Linker::RunInitializers() noexcept {
  for (u32 index = count_; index > 0U; --index) {
    const Object& object = objects_[index - 1U];
    if (object.init != 0U) {
      reinterpret_cast<void (*)()>(object.init)();
    }
    if (!RunInitArray(object)) {
      return false;
    }
  }
  return true;
}

bool Linker::RunInitArray(const Object& object) noexcept {
  if (object.init_array == 0U || object.init_array_size == 0U) {
    return true;
  }
  const u64 count = object.init_array_size / sizeof(u64);
  const auto* entries = reinterpret_cast<const u64*>(object.init_array);
  for (u64 index = 0; index < count; ++index) {
    if (entries[index] != 0U) {
      reinterpret_cast<void (*)()>(entries[index])();
    }
  }
  return true;
}

}  // namespace ldgem

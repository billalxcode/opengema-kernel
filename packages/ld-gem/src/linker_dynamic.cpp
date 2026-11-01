#include "ldgem/linker.h"

#include "ldgem/elf.h"

namespace ldgem {

namespace {

constexpr u32 kMaxDynamicEntries = 128;

}  // namespace

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

}  // namespace ldgem

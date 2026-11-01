#pragma once

// Small text helpers shared by the dynamic linker sources. They keep the
// freestanding build free of libc while avoiding a local copy in every file.

#include "ldgem/types.h"

namespace ldgem {

[[nodiscard]] u64 TextLength(const char* text) noexcept;

void WriteText(const char* text) noexcept;

void WriteText(const char* text, u64 size) noexcept;

void ReportSymbol(const char* prefix, const char* name) noexcept;

}  // namespace ldgem

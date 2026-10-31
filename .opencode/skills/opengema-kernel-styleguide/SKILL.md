---
name: opengema-kernel-styleguide
description: Styleguide kode opengema-kernel berbasis C++ Core Guidelines dan LLVM clang-format. Dipakai sebelum menulis atau edit file.
---

# opengema-kernel-styleguide

Sumber utama: C++ Core Guidelines (Stroustrup/Sutter), GSL, LLVM style.
Jangan mengarang aturan. Jika ragu, ikuti Core Guidelines SF, NL, ES, F, C, E.

## 1. Guard clause adalah aturan formatting

Semua fungsi dengan validasi harus early return. Maksimal 1 level nesting untuk path utama.

```cpp
Expected<uint64_t> MapPage(uint64_t va, uint64_t pa) {
  if (!IsAligned(va)) {
    return Unexpected{Status::Unaligned()};
  }
  if (!IsAligned(pa)) {
    return Unexpected{Status::Unaligned()};
  }
  // lanjut path utama
}
```

## 2. Layout file

```
kernel/
  boot/efi_main.cpp
  arch/aarch64/{uart_pl011.cpp, mmu.cpp, vectors.S, start.S}
  include/og/{status.h, mmio.h, uart.h, mmu.h}
  kernel/main.cpp
```

- Satu unit = satu `.h` + satu `.cpp`. Header hanya deklarasi dan `constexpr`.
- Header pakai `#pragma once` di baris pertama.
- Include order: own header, C++ freestanding, C, arch, project. Contoh:

```cpp
#include "og/uart.h"
#include <cstdint>
#include <span>
```

- Tidak ada implementasi besar di header kecuali `constexpr` atau template kecil.
- Path di dalam skill relatif terhadap folder skill, tapi path kode relatif terhadap repo root.

## 3. Naming

- Namespace: `snake_case` bertingkat: `og::arch::aarch64`.
- Type: `PascalCase`: `UartPl011`, `PageTable`, `TrapFrame`.
- Fungsi dan method: `snake_case`: `write_poll`, `map_range`.
- Variabel lokal: `snake_case`.
- Member: trailing underscore: `is_init_`, `mmio_base_`.
- Konstanta constexpr: `kPascalCase`: `kUartBase`, `kPageSize`.
- Enum value: `kPascalCase` dengan scope enum: `Status::kOk`.
- Macro: `UPPER_SNAKE` dan hanya untuk assembly atau linker: `OG_PAGE_SIZE`.
- File: `snake_case.cpp/.h`, asm `.S` kapital.

## 4. API shape

- Fungsi yang bisa gagal return `Status` atau `Expected<T>`, tandai `[[nodiscard]]`.
- Parameter output pakai `std::span` atau pointer non-null dengan kontrak. Jangan return raw owning pointer.
- Constructor tidak boleh gagal diam-diam. Jika init bisa gagal, sediakan `Init()` yang return `Status`, atau factory `Create()`.
- Destructor selalu `noexcept` dan tidak alokasi.
- Hindari getter setter trivial. Expose struct POD jika memang data.
- Komentar hanya untuk why, bukan what. What harus jelas dari nama dan tipe.

## 5. clang-format dan clang-tidy

Base: LLVM dengan penyesuaian kernel:

```yaml
BasedOnStyle: LLVM
ColumnLimit: 100
IndentWidth: 2
PointerAlignment: Left
AllowShortFunctionsOnASingleLine: Empty
SortIncludes: true
```

clang-tidy checks minimal:

```
cert-*,cppcoreguidelines-*,modernize-*,readability-*,
bugprone-*,performance-*,hicpp-*,clang-analyzer-*
```

Tolak kode yang melanggar: naked new/delete, narrowing tanpa `narrow_cast`, capturing `this` di lambda EL1 tanpa alasan, union tanpa variant.

## 6. Aturan diff

- Satu diff satu topik. Jangan campur driver + MMU + build.
- Edit existing file lebih diutamakan dari buat file baru.
- Tidak ada TODO tanpa pemilik. Format: `// TODO(owner): apa + kapan dihapus`.
- Setiap magic number harus named constant di `memory_map.h` atau `mair.h`.

## References

- Baca `references/guard-clause.md` untuk pola early return. Guard clause wajib di skill ini.
- Baca `references/naming-layout.md` untuk naming dan layout.
- Baca `references/clang-format-tidy.md` untuk format dan lint.

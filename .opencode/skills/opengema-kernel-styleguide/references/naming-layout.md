# Naming dan Layout

Basis: Core Guidelines NL, SF.

## Namespace dan type

- Namespace: `og::arch::aarch64`, snake_case.
- Type: `PascalCase`.
- Fungsi: `snake_case`.
- Member: trailing `_`.
- Konstanta: `kPascalCase`.
- Macro: `UPPER_SNAKE`, hanya glue asm dan linker.

## Layout file

- Satu `.h` + satu `.cpp` per unit.
- `#pragma once` baris pertama header.
- Order include: own header, C++ freestanding, C, arch, project.
- Tidak ada `using namespace` di header.
- Magic address hanya di `memory_map.h`.

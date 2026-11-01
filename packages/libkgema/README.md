# libkgema

SDK user space untuk opengema-kernel. Nama lengkapnya lib kernel open gema.
Library ini menyediakan titik masuk EL0 dan pembungkus syscall supaya
program seperti `gemabox` cukup menulis C++20 tanpa menyalin instruksi
`svc` dan tanpa libc.

## Isi

| Header | Fungsi |
|---|---|
| `kgema/types.h` | Tipe dasar `usize`, `u64`, `i64` tanpa header standar |
| `kgema/syscall.h` | Nomor syscall dan `invoke3` |
| `kgema/io.h` | `write` untuk buffer dan string literal |
| `kgema/memory.h` | `mmap` dan `munmap` |
| `kgema/process.h` | `exit` |

`src/start.cpp` menyediakan `_start` di section `.text.entry`. `link.ld`
menempatkannya tepat di 1 TiB, alamat yang dipakai kernel untuk user space.

## Pemakaian

```cpp
#include "kgema/io.h"

int main() {
  kgema::write("hello from el0\n");
  return 0;
}
```

Build dilakukan oleh CMake proyek utama. Hasilnya `build/libkgema.a`,
ditautkan bersama program memakai `packages/libkgema/link.ld`.

## Batasan

- Freestanding, tanpa libc dan tanpa STL.
- Tanpa exception dan RTTI.
- Global object dengan konstruktor dinamis belum dijalankan otomatis.
  Gunakan `constexpr`, `const`, atau static initialization.
- Satu program per address space pada satu waktu masih menjadi model
  eksekusi kernel saat ini.

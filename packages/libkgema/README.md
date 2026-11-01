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
| `kgema/file.h` | `open`, `close`, `read`, `write`, `seek`, `stat`, dan `chdir` |
| `kgema/memory.h` | `mmap` dan `munmap` |
| `kgema/process.h` | `getpid`, `spawn`, `wait`, dan `exit` |

`src/start.cpp` menyediakan `_start` di section `.text.entry`. `link.ld`
menempatkannya tepat di 1 TiB, alamat yang dipakai kernel untuk user space.

## Pemakaian

```cpp
#include "kgema/file.h"
#include "kgema/io.h"

int main(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  (void)envp;
  kgema::write("hello from el0\n");
  const kgema::i64 fd = kgema::open("/ram/hello");
  if (fd >= 0) {
    char text[32];
    (void)kgema::read(fd, text, sizeof(text));
    (void)kgema::close(fd);
  }
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
- Setiap proses punya satu address space dan satu thread user saat ini.
- Descriptor 0, 1, dan 2 dicadangkan untuk stdio. Berkas mulai dari fd 3.
- `read` pada fd 0 membaca satu baris dari UART dengan echo, backspace, dan
  blocking sampai newline.

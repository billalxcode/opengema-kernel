# libkgema

Lapisan ABI kernel untuk user space opengema-kernel. Library ini privat:
hanya `libgemapi` yang boleh memakainya. Aplikasi memakai `libgemapi`.

Isi library ini adalah pembungkus syscall mentah dan helper ABI. Tidak ada
kebijakan aplikasi, validasi API, atau titik masuk program di sini. Titik
masuk `_start` berada di `libgemapi`.

## Isi

| Header | Fungsi |
|---|---|
| `kgema/types.h` | Tipe dasar `usize`, `u64`, `i64` tanpa header standar |
| `kgema/syscall.h` | Nomor syscall dan `invoke3` |
| `kgema/io.h` | `write` untuk buffer dan string literal |
| `kgema/file.h` | `open`, `close`, `read`, `write`, `seek`, `stat`, dan `chdir` |
| `kgema/memory.h` | `mmap`, `mmap_executable`, dan `munmap` |
| `kgema/process.h` | `getpid`, `spawn`, `wait`, dan `exit` |

`src/runtime.cpp` menyediakan `memset`, `memcpy`, dan `memmove` yang mungkin
dihasilkan compiler untuk inisialisasi agregat. Ini bukan syscall dan bukan
API aplikasi.

`src/syscall.cpp` adalah satu-satunya tempat instruksi `svc` di seluruh
proyek. `scripts/check-gsl.sh` menolak `svc` di paket lain.

## Aturan pemakaian

- Hanya `packages/libgemapi/src` yang boleh menyertakan `kgema/...`.
- Aplikasi tidak boleh menyertakan header ini.
- Aplikasi tidak boleh menautkan `libkgema.a` atau `libkgema.so`.
- `libkgema` tidak boleh menyertakan header `gemapi/` atau header kernel.

## Build

CMake proyek utama menghasilkan:

```
build/libkgema.a
build/libkgema.so.1
build/libkgema.so -> libkgema.so.1
```

`libkgema.so.1` adalah dependency dari `libgemapi.so.1`. Dynamic linker
`ld-gem` memuatnya dari `/system/libraries` pada tahap GSL berikutnya.

## Batasan

- Freestanding, tanpa libc dan tanpa STL.
- Tanpa exception dan RTTI.
- Setiap proses punya satu address space dan satu thread user saat ini.
- Descriptor 0, 1, dan 2 dicadangkan untuk stdio. Berkas mulai dari fd 3.
- `read` pada fd 0 membaca satu baris dari UART dengan echo, backspace, dan
  blocking sampai newline.
- `mmap_executable` hanya dipakai dynamic linker. Aplikasi biasa memakai
  `gemapi::memory::map` yang tidak executable.

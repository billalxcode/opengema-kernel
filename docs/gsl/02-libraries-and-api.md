# 02 - Library dan API

Dokumen ini membahas pembagian `libkgema` dan `libgemapi`, API publik, serta
strategi static dan dynamic library.

## 1. libkgema

`libkgema` adalah lapisan ABI kernel untuk user space. Nama lengkapnya lib
kernel gema.

Isi yang diusulkan:

- Nomor syscall dan struktur ABI.
- Pembungkus `svc` untuk write, read, open, close, mmap, spawn, wait,
  chdir, dan seterusnya.
- Startup minimal `_start`.
- Helper dasar yang dibutuhkan ABI: copy, span, result, dan error.

Aturan:

- Hanya library ini yang boleh memakai inline assembly `svc`.
- Header `kgema/` tidak diberikan kepada aplikasi biasa.
- `libkgema` tidak mengatur kebijakan, sandbox, atau format package.
- `libkgema` boleh berubah mengikuti ABI kernel.

`libgemapi` memakai `libkgema` secara privat. Aplikasi tidak boleh
menautkan `libkgema` langsung.

Pengecualian: `ld-gem` adalah bagian layer libraries, bukan aplikasi. Ia
memakai `libkgema` langsung karena harus berjalan sebelum `libgemapi`
tersedia.

## 1.1 Struktur paket

```
packages/
  libkgema/       ABI kernel, privat, hanya untuk libgemapi
    include/kgema/
    src/
  libgemapi/      API publik GSL
    include/gemapi/
    src/
  gemabox/        aplikasi multi-call
    main.cpp
  apps/           aplikasi tambahan nanti
```

Aturan build:

- `gemabox` hanya menambahkan include `packages/libgemapi/include`.
- `libgemapi` menambahkan include `packages/libkgema/include` secara privat.
- `libkgema` tidak menambahkan include `packages/libgemapi`.

## 2. libgemapi

`libgemapi` adalah API publik GSL. Aplikasi menautkan library ini.

Modul yang diusulkan:

| Modul | Isi |
|---|---|
| `gemapi/io` | Console, stdin, stdout, stderr |
| `gemapi/fs` | Path, file, directory, stat, mount |
| `gemapi/process` | Spawn, exec, wait, exit, PID |
| `gemapi/memory` | mmap, munmap, heap user, shared memory |
| `gemapi/time` | Waktu, sleep, timeout |
| `gemapi/system` | Versi, capability, shutdown, informasi sistem |
| `gemapi/error` | Status, Result, kode error |

Fitur yang wajib ada di API:

- Handle file dan proses yang buram.
- Validasi ukuran buffer dan path.
- `Result<T>` atau `Status` untuk semua operasi fallible.
- Tidak ada exception, RTTI, atau alokasi tersembunyi.
- Error tidak boleh berupa angka mentah tanpa dokumentasi.

Contoh pemakaian yang diusulkan:

```cpp
#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  (void)envp;

  auto file = gemapi::fs::open("/system/config/version");
  if (!file) {
    gemapi::io::error("cannot open version\n");
    return 1;
  }
  char buffer[64];
  auto count = file->read(buffer, sizeof(buffer));
  if (count) {
    gemapi::io::write(buffer, count.value());
  }
  return 0;
}
```

## 3. Runtime

Runtime kecil diperlukan untuk:

- `_start`, `init_array`, dan entry ke `main`.
- Argv dan envp.
- Panic user dan exit yang bersih.
- Alokasi user yang sederhana.

Usulan awal:

- Runtime berada di `libgemapi` atau library terpisah `libgemrt`.
- Tanpa exception dan RTTI.
- Konstruktor global sederhana boleh berjalan, destructor statis tidak
  dijanjikan.
- Tidak ada libc penuh.

## 4. Dynamic library

Keputusan brainstorming: aplikasi memakai dynamic library langsung.

- Aplikasi ELF memiliki `DT_NEEDED` untuk `libgemapi.so`.
- `libgemapi.so` memiliki dependency `libkgema.so`.
- Kernel memuat executable dan membaca `PT_INTERP`.
- Dynamic linker user space adalah program statis bernama `ld-gem`.
- `ld-gem` membaca dependency dari `/system/libraries`.
- Simbol diselesaikan dengan versi dan urutan yang jelas.

Yang dibutuhkan:

- ELF64 loader dan `mmap` file.
- Shared object loader, relokasi, symbol table, dan `DT_NEEDED`.
- Aturan versi library dan soname.
- Read-only mapping untuk code dan relro.
- Search path library yang tidak bisa diubah aplikasi.

Pengecualian bootstrap:

- `ld-gem` sendiri statis karena harus bisa berjalan sebelum ada library
  dinamis.
- Init boleh dinamis setelah linker stabil. Jika belum, init statis
  sementara adalah satu satunya pengecualian aplikasi.
- gemabox dan aplikasi biasa tetap dynamic.

Konsekuensi:

- Dynamic loader harus selesai sebelum aplikasi GSL bisa berjalan.
- Build aplikasi membutuhkan `libgemapi.so`, `libkgema.so`, dan `ld-gem`.
- ABI library harus dijaga sejak awal karena semua aplikasi memakainya.


## 5. Aturan link

Usulan aturan build:

1. Aplikasi hanya boleh `#include "gemapi/..."`.
2. Aplikasi hanya boleh menautkan `libgemapi`.
3. `libgemapi` menautkan `libkgema` secara privat.
4. Simbol `kgema` tidak boleh diekspor ke aplikasi.
5. Aplikasi tidak boleh memiliki inline assembly `svc`.
6. `libkgema` tidak boleh menyertakan header kernel internal.

Pemeriksaan bisa dilakukan oleh `scripts/check-gsl.sh` dan link map.

## 6. Versi API

API perlu versi agar aplikasi lama tidak langsung rusak.

Usulan:

- Versi mayor dan minor di header.
- Simbol library memakai versi mayor.
- Perubahan yang memutus kompatibilitas menaikkan versi mayor.
- Aplikasi mencantumkan dependency minimum di manifest.

## 7. Yang belum diputuskan

- Nama namespace final: `gemapi`, `gema`, atau `gsl`.
- Apakah runtime dipisah dari `libgemapi`.
- Apakah file dan device memakai handle atau path.
- Apakah API menyediakan thread atau hanya proses.
- Apakah library boleh melakukan syscall sendiri lewat `libkgema`.

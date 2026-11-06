# ld-gem

Dynamic linker GEMA. Program statis yang dijalankan kernel sebagai
interpreter ELF ketika executable memiliki `PT_INTERP`.

`ld-gem` adalah satu-satunya program statis jangka panjang karena harus
berjalan sebelum shared object tersedia. Ia memakai `libkgema` langsung
sebagai bagian dari layer libraries, bukan `libgemapi`.

## Alur

1. Kernel memvalidasi executable, memetakan segmen `PT_LOAD`, dan membaca
   `PT_INTERP`.
2. Kernel memilih load bias acak untuk executable PIE/ET_DYN, memuat
   `/system/boot/ld-gem` di base fixed, lalu menyerahkan `GEM_DYNAMIC`,
   `GEM_ENTRY`, dan `GEM_BASE` lewat environment.
3. `ld-gem` membaca dynamic section executable utama dengan base yang
   diberikan kernel.
4. Setiap `DT_NEEDED` dimuat dari search path, lalu dependensinya diproses
   rekursif.
5. Relokasi `R_AARCH64_RELATIVE`, `GLOB_DAT`, `JUMP_SLOT`, dan `ABS64`
   diterapkan.
6. Initializer library dijalankan.
7. `ld-gem` melompat ke entry point executable dengan argc, argv, dan envp
   yang sama.

## Isi

| Berkas | Fungsi |
|---|---|
| `include/ldgem/elf.h` | Konstanta ELF64, tag dynamic, dan pembaca byte |
| `include/ldgem/image.h` | Parsing dan pemetaan shared object |
| `include/ldgem/linker.h` | Scope simbol, relokasi, dan initializer |
| `src/elf.cpp` | Pembaca little-endian dan hash SYSV |
| `src/image.cpp` | `mmap` writable, salin `PT_LOAD`, `mprotect` per segmen, load bias |
| `src/linker.cpp` | `DT_NEEDED`, symbol lookup, relokasi |
| `src/main.cpp` | Pembacaan environment dan orkestrasi |
| `src/start.cpp` | Titik masuk EL0 statis |
| `link.ld` | Base `0x10004000000` (1 TiB + 64 MiB), di atas mmap executable |

## Batasan saat ini

- Search path `/system/libraries` di volume system GSLFS.
- Hanya hash SYSV (`DT_HASH`) yang dipakai untuk lookup simbol. Library
  harus dibangun dengan hash SYSV, yang merupakan default `ld` di proyek
  ini.
- Hanya `RELA`, belum ada `REL`.
- Belum ada lazy binding; semua relokasi eager.
- Belum ada symbol versioning, `dlopen`, atau TLS.
- Library dan executable dipetakan writable dan executable. W^X user
  menyusul pada hardening.
- `ld-gem` sendiri statis dan tidak memakai `libgemapi`.

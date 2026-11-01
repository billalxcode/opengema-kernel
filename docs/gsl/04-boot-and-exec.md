# 04 - Boot, Init, dan Exec

Dokumen ini menjelaskan bagaimana kernel menyerahkan sistem ke user space
dan bagaimana binary seperti gemabox dijalankan dari filesystem.

## 1. Prinsip

1. Kernel menyiapkan hardware, memori, interrupt, scheduler, dan VFS.
2. Kernel tidak mengenal gemabox secara khusus.
3. Kernel memuat program pertama dari filesystem.
4. Init memasang volume dan menjalankan service serta shell.
5. Aplikasi dijalankan lewat API proses, bukan ditanam di kernel.

## 2. Alur boot

```
Firmware UEFI
  -> BOOTAA64.EFI
  -> kmain
  -> init hardware, MMU, heap, interrupt, timer
  -> driver block dan VFS read-only
  -> mount root/system minimal
  -> load /System/Boot/init
  -> PID 1 user mode
       -> mount /System, /Applications, /Data
       -> start service manager
       -> start shell atau user interface
```

Kernel hanya perlu filesystem yang cukup untuk membaca init. Setelah init
berjalan, mounting tambahan dilakukan user space melalui API mount.

## 3. Mengapa init di user space

- Kebijakan mount bukan tugas kernel.
- Service dapat di-restart tanpa reboot.
- Kesalahan service tidak mematikan kernel.
- Layout filesystem dapat berubah tanpa mengubah kernel.
- Init dapat memilih shell grafis atau text.

Kernel tetap memiliki VFS dan syscall mount. Yang dipindahkan adalah
kebijakan dan urutan mount.

## 4. Alur exec

Usulan alur `spawn("/Applications/gemabox", argv)`:

1. libgemapi memvalidasi path, argv, dan capability.
2. libgemapi memanggil `libkgema`.
3. `libkgema` memanggil syscall spawn.
4. Kernel mencari binary di VFS.
5. Kernel membaca header ELF dan memvalidasi segment.
6. Kernel membuat address space baru.
7. Kernel memetakan segment read-only, data, dan stack.
8. Kernel menyalin argv/env ke stack user.
9. Kernel membuat proses dan thread user.
10. Jika dynamic, kernel menjalankan dynamic linker.
11. Dynamic linker memuat `libgemapi.so` dan `libkgema.so` dari
    `/System/Libraries`.
12. Program masuk ke `_start`, lalu `main`.

Jika salah satu langkah gagal, proses lama tidak berubah dan error
dikembalikan ke pemanggil.

## 5. Exec versus spawn

- `spawn`: membuat proses baru, parent tetap hidup, mengembalikan PID.
- `exec`: mengganti image proses saat ini, PID tetap.
- Shell memakai `spawn` untuk command biasa.
- Init memakai `spawn` untuk service.
- `exec` berguna untuk shell yang ingin mengganti dirinya dengan command.

Tahap awal cukup `spawn` dan `wait`. `exec` menyusul setelah loader stabil.

## 6. Dynamic library

Keputusan brainstorming: aplikasi memakai dynamic library langsung.

- Kernel memuat executable ELF dan membaca `PT_INTERP`.
- Interpreter adalah `ld-gem`, dynamic linker statis.
- `ld-gem` membaca `DT_NEEDED` dari executable.
- `libgemapi.so` bergantung pada `libkgema.so`.
- `ld-gem` mencari library hanya di `/System/Libraries`.
- Loader memvalidasi versi, soname, dan dependensi.
- Segment code dipetakan read-only dan executable.
- Relokasi dan symbol binding diselesaikan sebelum entry.
- Library yang hilang atau versi salah menghasilkan error exec.

Kernel tidak perlu memahami simbol library. Kernel memvalidasi ELF,
memetakan file, dan menyediakan syscall yang dibutuhkan `ld-gem`.

### Antarmuka kernel ke ld-gem

Kernel belum memakai auxiliary vector penuh. Setelah memetakan executable
utama dan interpreter, kernel menambahkan dua environment ke stack user:

```
GEM_DYNAMIC=0x<alamat dynamic section>
GEM_ENTRY=0x<entry point executable>
```

`ld-gem` membaca kedua nilai itu, memuat `DT_NEEDED`, menerapkan relokasi,
lalu melompat ke `GEM_ENTRY` dengan argc, argv, dan envp yang sama.

Untuk memetakan shared object, `ld-gem` memakai `mmap` dengan flag
executable. Aplikasi biasa tidak memakai flag itu; `gemapi::memory::map`
tetap non-executable.

Selama bootstrap, `ld-gem` mencari library di `/ram`. Setelah GSLFS dan
init user ada, search path menjadi `/System/Libraries`.

Pengecualian bootstrap:

- `ld-gem` statis karena harus berjalan sebelum library dinamis tersedia.
- Init boleh dinamis setelah linker stabil. Init statis sementara hanya
  boleh sebagai fallback bring-up, bukan target akhir.
- gemabox dan aplikasi biasa wajib dynamic.

## 7. Kegagalan yang harus ditangani

| Kegagalan | Hasil |
|---|---|
| Binary tidak ada | spawn gagal, error ke pemanggil |
| ELF rusak | spawn gagal, tidak ada proses baru |
| Library hilang | loader gagal, proses baru dibatalkan |
| Alokasi memori gagal | proses baru dibatalkan |
| Program crash | proses berhenti, shell tetap hidup |
| Init crash | kernel dapat restart init atau panic terkontrol |
| System volume rusak | fallback ke recovery atau shell minimal |

## 8. Yang masih dibahas

- Bagaimana recovery jika init gagal.
- Apakah kernel menyediakan `execve` langsung atau hanya `spawn`.
- Apakah `ld-gem` juga menangani lazy binding atau semua eager.
- Format metadata versi library.

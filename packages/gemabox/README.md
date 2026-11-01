# gemabox

Program user mode dasar untuk opengema-kernel. Ditulis dengan C++20 di atas
API publik `libgemapi`, dikompilasi menjadi `build/gemabox.elf`, disalin ke
ESP sebagai `GEMABOX.ELF`, lalu dibaca kernel ke ramfs sebagai
`/ram/gemabox` sebelum dijalankan di EL0. Kernel memvalidasi header ELF64,
memetakan setiap segmen `PT_LOAD`, dan memakai alamat entry dari file.

Aplikasi ini tidak menyertakan header `kgema/` dan tidak menautkan
`libkgema`. Lapisan ABI itu dipakai `libgemapi` secara privat.

## Berkas

| Berkas | Isi |
|---|---|
| `main.cpp` | Program utama |
| `README.md` | Dokumen ini |

Entry point `_start`, pembungkus syscall, dan helper output disediakan oleh
`packages/libgemapi`. Program cukup menulis `main()`. Hanya instruksi
`svc #0` di dalam `libkgema` yang memakai inline assembly, karena itu batas
ABI yang tidak bisa dihindari. Tidak ada file `.S`.

## Isi saat ini

1. `write` ke stdout.
2. `open`, `read`, `stat`, `seek`, dan `close` untuk `/ram/hello`.
3. `mmap` satu halaman.
4. `write` dari halaman hasil pemetaan.
5. `munmap` halaman tersebut.
6. `spawn` child dengan argv `{"gemabox", "child"}`, lalu `wait`.
7. Mode `writer`: `chdir("/ram")`, tulis `write ok` ke file `writer`,
   seek, read, lalu cetak `write ok`.
8. Mode `shell`: prompt `gemabox>`, baca input UART, jalankan `help`,
   `echo`, `cat`, dan `exit`.
9. `exit`.

Child mendeteksi argv kedua dan hanya mencetak `child ok`.

Hasil yang diharapkan di serial:

```
hello from el0
file ok
mmap ok
child ok
spawn ok
write ok
gemabox> help
commands: help, echo, cat, exit
gemabox> exit
user: exit pid: 1
user: exit status: 0
```

## Rencana

gemabox akan menjadi kumpulan perintah dasar kernel, misalnya `ls`, `cat`,
`mount`, `ps`, dan `help`, dengan satu titik masuk `_start` yang memilih
perintah dari argumen. Argv dan spawn sudah ada; `exec` dan input interaktif
belum.

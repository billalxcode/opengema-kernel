# gemabox

Program user mode dasar untuk opengema-kernel. Ditulis dengan C++20 di atas
API publik `libgemapi`. Binary yang dipasang adalah ELF64 dynamic:
`PT_INTERP` menunjuk `/system/boot/ld-gem` dan `DT_NEEDED` menunjuk
`libgemapi.so.1`. Kernel memvalidasi ELF, memetakan segmen `PT_LOAD`, lalu
`ld-gem` memuat dependency dari filesystem sebelum masuk ke `main`.

Saat bring-up, binary berada di volume system GSLFS sebagai
`/applications/gemabox/gemabox`; kernel memvalidasi ELF dan memetakannya
sebelum `ld-gem` memuat library.

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
2. `open`, `read`, `stat`, `seek`, dan `close` untuk `/tmp/hello`.
3. `mmap` satu halaman.
4. `write` dari halaman hasil pemetaan.
5. `munmap` halaman tersebut.
6. `spawn` child dengan argv `{"gemabox", "child"}`, lalu `wait`.
7. Mode `writer`: `chdir("/tmp")`, tulis `write ok` ke file `writer`,
   seek, read, lalu cetak `write ok`.
8. Mode `shell`: prompt `gemabox>`, baca input UART, jalankan `help`,
   `echo`, `cat`, `ls`, `clear`, dan `exit`.
9. Command di luar applet dicari di `/commands/<name>`, lalu
   `/applications/<name>/<name>`.
10. `exit`.

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
commands: help, echo, cat, ls, clear, exit
gemabox> hello
hello from applications
gemabox> exit
user: exit pid: 1
user: exit status: 0
```

## Rencana

gemabox tetap menjadi multi-call binary untuk command inti. Applet `ps`,
`sleep`, dan `mount` menyusul. Aplikasi besar tetap binary terpisah di
`/applications`.

# gemabox

Program user mode dasar untuk opengema-kernel. Ditulis dengan C++20 di atas
SDK `libkgema`, dikompilasi menjadi `build/gemabox.bin`, disalin ke ESP
sebagai `GEMABOX.BIN`, lalu dibaca kernel ke ramfs sebagai `/ram/gemabox`
sebelum dijalankan di EL0.

## Berkas

| Berkas | Isi |
|---|---|
| `main.cpp` | Program utama |
| `README.md` | Dokumen ini |

Entry point `_start`, pembungkus syscall, dan helper output disediakan oleh
`packages/libkgema`. Program cukup menulis `main()`. Hanya instruksi
`svc #0` di dalam library yang memakai inline assembly, karena itu batas
ABI yang tidak bisa dihindari. Tidak ada file `.S`.

## Isi saat ini

1. `write` ke stdout.
2. `mmap` satu halaman.
3. `write` dari halaman hasil pemetaan.
4. `munmap` halaman tersebut.
5. `exit`.

Hasil yang diharapkan di serial:

```
hello from el0
mmap ok
user: exit ok
```

## Rencana

gemabox akan menjadi kumpulan perintah dasar kernel, misalnya `ls`, `cat`,
`mount`, `ps`, dan `help`, dengan satu titik masuk `_start` yang memilih
perintah dari argumen. Selama kernel belum punya `exec` dan argumen proses,
isinya masih berupa program uji tunggal.

# gemabox

Program user mode dasar untuk opengema-kernel. Ditulis dalam C freestanding,
dikompilasi menjadi `build/gemabox.bin`, disalin ke ESP sebagai
`GEMABOX.BIN`, lalu dibaca kernel ke ramfs sebagai `/ram/gemabox` sebelum
dijalankan di EL0.

## Berkas

| Berkas | Isi |
|---|---|
| `main.c` | Program utama dan pembungkus syscall |
| `link.ld` | Menempatkan `_start` di 1 TiB dan menyusun section |
| `README.md` | Dokumen ini |

Hanya instruksi `svc #0` di pembungkus syscall yang memakai inline
assembly. Logika program seluruhnya C. Tidak ada startup libc dan tidak ada
file `.S`.

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

## ABI syscall

Nomor syscall di `x8`, argumen di `x0` sampai `x2`, hasil di `x0`.
Daftar nomor ada di `kernel/include/og/syscall/syscall.h`.

## Rencana

gemabox akan menjadi kumpulan perintah dasar kernel, misalnya `ls`, `cat`,
`mount`, `ps`, dan `help`, dengan satu titik masuk `_start` yang memilih
perintah dari argumen. Selama kernel belum punya `exec` dan argumen proses,
isinya masih berupa program uji tunggal.

# hello

Aplikasi eksternal pertama untuk menguji command lookup dari shell.

Shell mencari command di `/commands/<name>`, lalu di
`/applications/<name>/<name>`. `hello` berada di
`/applications/hello/hello` dan mencetak:

```
hello from applications
```

Binary ini dynamic seperti aplikasi lain:

```
PT_INTERP /system/boot/ld-gem
DT_NEEDED libgemapi.so.1
```

Manifest ada di `packages/system-root/applications/hello/manifest.txt`.

## Mode pengujian

- `hello` mencetak `hello from applications`.
- `hello fault` menyentuh alamat 0 untuk menguji fault user dari EL0.
- `hello badptr` membuka path dengan pointer `1` untuk menguji penolakan
  pointer invalid.
- `hello wx` menulis ke halaman code untuk memastikan W^X menolak.
- `hello fuzz` menjalankan 512 panggilan filesystem, proses, I/O, dan memori
  dengan pointer serta ukuran acak deterministik. Program harus hidup sampai
  mencetak `FUZZ-DONE`.
- `hello mem` memetakan 8 MiB lalu melepasnya, kemudian memastikan map 40 MiB
  ditolak oleh limit 32 MiB. Program mencetak `mem: limit enforced`.
- `hello aslr` mencetak alamat stack, mmap, dan code. Dijalankan dua kali
  oleh tes untuk memastikan ketiga alamat berubah. Aplikasi dynamic dibangun
  sebagai PIE/ET_DYN dan kernel memilih load bias acak.
- `hello stack` merekursi dengan frame 4 KiB sampai melewati stack awal
  16 KiB, lalu harus tumbuh dan mencetak `stack: grew`.
- `hello fork` membuka `/system/config/version`, memanggil `fork`, lalu
  child menulis global COW dan membaca berkas; parent memastikan global
  lamanya tidak berubah dan membaca berkas dari offset FD sendiri.
- `hello exec` mengganti image dirinya dengan `/applications/hello/hello`
  dan argv `exec-child`. Image baru mencetak `exec: child`, lalu shell
  melanjutkan setelah proses lama diganti.
- `hello lazy` memetakan 8 MiB secara lazy, menulis halaman pertama dan
  terakhir, lalu memastikan `/proc/meminfo` mencatat `lazy_faults: 2`.
- `hello errno` memeriksa pemetaan errno: path hilang menjadi `not found`,
  pointer invalid menjadi `invalid argument`, dan map 40 MiB menjadi
  `out of memory`.
- `hello stackguard` merekursi melewati batas growth 64 KiB. Proses harus
  mati pada guard page, bukan mencetak `stack: guard missing`.

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

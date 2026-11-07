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
- `hello nx` menyalin instruksi `ret` ke halaman data RW NX lalu
  memanggilnya; proses harus mati dan shell tetap berjalan.
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
- `hello shm` memetakan satu halaman shared, lalu `fork`. Child menulis
  `0x22` dan parent harus melihat nilai yang sama setelah `wait`.
- `hello futex` memakai shared page, child menunggu `futex_wait` pada nilai
  0, lalu parent mengubah nilai menjadi 1 dan `futex_wake`. Parent harus
  membangunkan tepat satu waiter dan child mencetak `futex: child woke`.
- `hello tls` mengatur TPIDR_EL0 ke blok TLS, memaksa context switch,
  memastikan register dan isi blok tetap sama, lalu fork dan memastikan
  child mewarisi nilai TLS parent.
- `hello filemap` membuka `/system/config/version`, memetakan snapshot
  private 4 KiB, memeriksa isi dan zero tail, lalu memastikan tulisan ke
  mapping tidak mengubah berkas asli. Library ELF multi-page juga dipetakan
  untuk memeriksa magic ELF dan unmap.
- `hello prio` membaca prioritas awal 16, menurunkannya ke 20, menolak
  nilai 40, lalu memastikan proses root bisa menaikkan prioritas dan proses
  uid 1 tidak bisa.
- `hello stackguard` merekursi melewati batas growth 64 KiB. Proses harus
  mati pada guard page, bukan mencetak `stack: guard missing`.

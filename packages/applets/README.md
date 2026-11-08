# Applets

Biner ELF kecil yang dijalankan gemabox dari `/bin`. Setiap applet memakai
`libgemapi` dan tidak menyertakan header kernel atau `libkgema`.

Slice pertama:

| Applet | Fungsi |
|---|---|
| `echo` | Cetak argumen |
| `cat` | Cetak isi berkas |
| `ls` | Daftar direktori |
| `pwd` | Cetak cwd |
| `touch` | Buat berkas kosong |
| `head` | Sepuluh baris pertama |
| `wc` | Hitung baris dan byte |
| `grep` | Cetak baris yang cocok |
| `free` | Baca `/proc/meminfo` |
| `pkg` | Baca `/system/packages/registry` |

Perintah yang mengubah state shell atau sistem tetap built-in di gemabox:
`cd`, `setuid`, `setgid`, `prio`, `mount`, `umount`, `date`, `tz`, dan
`loglevel`. `cd` tidak dapat menjadi proses terpisah tanpa protokol shell
khusus.

Registry package berada di `/system/packages/registry` dengan format
`name<TAB>version<TAB>author<TAB>description<TAB>exec`. Lihat
`docs/PACKAGE-REGISTRY.md`.

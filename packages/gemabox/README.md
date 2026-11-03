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
8. Mode `shell`: prompt `gemabox>`, baca input UART dan keyboard,
   jalankan `help`, `echo`, `cat`, `ls`, `clear`, `acpi`, `uptime`,
   `sleep`, `ps`, `dmesg`, `loglevel`, `mkdir`, `rm`, `mv`, `mount`,
   `umount`, `halt`, `shutdown`, dan `reboot`.
9. Command di luar applet dicari di `/commands/<name>`, lalu
   `/applications/<name>/<name>`.
10. `acpi` membaca ringkasan tabel dari `/proc/acpi`. `uptime` mencetak
    waktu sejak boot, `sleep` menunggu dalam milidetik, `ps`
    menampilkan daftar proses dari `/proc/processes`, `dmesg`
    membaca ring log kernel dari `/proc/dmesg`, dan `loglevel`
    menampilkan atau mengubah filter level log. `mkdir`, `rm`, dan `mv`
    mengubah isi filesystem writable. `mount ramfs <path>` memasang ramfs
    baru dengan referensi global, dan `umount <path>` melepas referensi
    proses tersebut. `cat /proc/block` menampilkan nama dan kapasitas
    device blok. Pengujian persistensi FAT32 ada di aplikasi terpisah
    `/applications/fstest/fstest`, bukan command shell.
    `halt` menghentikan shell dan init sehingga kernel menyelesaikan self
    test lalu berhenti. `shutdown` dan `reboot` memakai ACPI/PSCI untuk
    mematikan atau me-reboot mesin.
11. Path dengan spasi bisa ditulis di dalam tanda kutip ganda, misalnya
    `mkdir "/tmp/quoted dir"`.

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
commands: help, echo, cat, ls, clear, acpi, uptime, sleep, ps, dmesg, loglevel, mkdir, rm, mv, mount, umount, halt, shutdown, reboot
gemabox> acpi
ACPI revision 2
gemabox> uptime
uptime: 12345 ms
gemabox> sleep 50
gemabox> ps
pid ppid state name
1 0 ready /system/boot/init
...
gemabox> dmesg
dmesg:
kernel: kmain entered
...
gemabox> loglevel
loglevel: info (2)
gemabox> mkdir /tmp/demo
gemabox> mv /tmp/demo /tmp/demo2
gemabox> rm /tmp/demo2
gemabox> mkdir /tmp/mnt
gemabox> mount ramfs /tmp/mnt
mount: mounted
gemabox> umount /tmp/mnt
umount: unmounted
gemabox> ...
gemabox> hello
hello from applications
gemabox> halt
user: exit pid: 1
user: exit status: 0
```

## Rencana

gemabox tetap menjadi multi-call binary untuk command inti. Applet `ps`,
`sleep`, dan `mount` menyusul. Aplikasi besar tetap binary terpisah di
`/applications`.

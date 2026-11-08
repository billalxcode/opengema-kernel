# Package registry dan applet terpisah

Status: slice pertama dikerjakan.

## Tujuan

1. Setiap program dan applet adalah ELF terpisah, bukan cabang `if` di
   dalam gemabox.
2. Gemabox menjadi shell daemon kecil: membaca baris, menjalankan built-in
   yang mengubah state shell, lalu mengeksekusi biner dari `/bin`.
3. Metadata package dicatat di registry read-only `/system/packages/registry`.
4. `cd`, `setuid`, `setgid`, `prio`, `mount`, `umount`, `tz`, `date`, dan
   `loglevel` tetap built-in karena mengubah state proses atau sistem.
   `cd` tidak bisa menjadi proses terpisah tanpa protokol shell khusus.

## Format registry

Satu baris per package, field dipisah tab:

```text
name<TAB>version<TAB>author<TAB>description<TAB>exec
```

Contoh:

```text
echo	0.1	opengema	Echo arguments	/bin/echo
gemabox	0.1	opengema	Shell daemon	/bin/gemabox
```

Registry bersifat read-only dan berada di volume GSLFS. Package manager
yang menulis metadata, signature, dan dependency resolution belum menjadi
bagian slice ini.

## Roadmap

- [x] PKG-01 Format registry dan file `/system/packages/registry`.
- [x] PKG-02 Metadata author, versi, deskripsi, dan exec untuk package
      yang ada.
- [x] PKG-03 Applet `pkg` dengan `list` dan `info <name>`.
- [x] APP-01 Layout `/bin` dan pemindahan gemabox ke `/bin/gemabox`.
- [x] APP-02 Applet ELF terpisah untuk `echo`, `cat`, `ls`, `pwd`, `touch`,
      `head`, `wc`, `grep`, `free`, dan `pkg`.
- [~] APP-03 Migrasi `acpi`, `uptime`, `ps`, `dmesg`, `id`, `stat`,
      `sleep`, `mkdir`, `rm`, `mv`, `chmod`, `chown`, `date`, `tz`,
      `shutdown`, `reboot`, dan `halt` ke `/bin`.
- [~] APP-04 Shell lookup `/bin`, daftar help, dan package listing.
      `which` dan package dependency resolution belum.
- [ ] PKG-04 Package write, signature, dan dependency resolution.

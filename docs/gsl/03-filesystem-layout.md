# 03 - Layout Filesystem

Dokumen ini mengusulkan layout filesystem GSL. Tujuannya mirip Linux atau
Windows, tetapi tetap cocok dengan konsep layer dan mounting.

## 1. Istilah

- Mount: memasang volume atau filesystem ke sebuah path.
- Volume: satu filesystem yang bisa dipasang, misalnya system, apps, data.
- Load: memetakan binary atau library dari filesystem ke proses.
- Package: direktori aplikasi berisi binary, manifest, dan resource.

Library tidak perlu di-mount satu per satu. Yang di-mount adalah volume
`System`, lalu library di-load dari volume itu.

## 2. Pilihan gaya

### Linux-like

```
/System
/Applications
/Devices
/Processes
/Volumes
/Home
/Temp
```

Kelebihan:

- Satu root, mount point jelas.
- Cocok dengan VFS yang sudah ada.
- Tidak perlu drive letter di kernel.

### Windows-like

```
C:\System
C:\Applications
D:\Data
```

Kelebihan:

- Familiar untuk pengguna Windows.

Kekurangan:

- Drive letter harus dipetakan ke mount table.
- Path dan escape menjadi lebih rumit.
- Kurang cocok dengan VFS Unix yang sudah ada.

Rekomendasi: gunakan satu root Linux-like. Jika perlu kompatibilitas
Windows, sediakan `/Volumes/C` atau antarmuka terjemahan, bukan drive letter
di kernel.

## 3. Layout usulan

```
/
  System/
    Boot/
      init                 proses pertama dari kernel
    Libraries/
      libkgema.so
      libgemapi.so
      libgemrt.so
    Services/
      service-manager
      device-manager
    Config/
      gsl.conf
      version
    Commands/              command inti opsional
  Applications/
    gemabox/
      gemabox              ELF executable
      manifest.txt         dependency dan metadata
      resources/
    editor/
    files/
  Commands/                link atau daftar applet
  Devices/
    console
    null
    random
    blk0
  Processes/
    <pid>/
      cmdline
      status
  Volumes/
    boot/
    system/
    data/
  Home/
  Temp/
  Var/
    Log/
```

## 4. Aturan mount

1. `/System` adalah volume read-only yang dipasang saat init.
2. `/Applications` dapat berada di volume read-only yang sama atau volume
   terpisah.
3. `/Home` dan `/Data` adalah volume baca-tulis.
4. `/Devices` dan `/Processes` adalah filesystem virtual dari kernel.
5. `/Volumes` adalah tempat volume tambahan.
6. Mount table berada di kernel, kebijakan mount ada di init atau service
   manager.

## 5. Binary dan package

Setiap aplikasi berada di direktori package:

```
/Applications/gemabox/
  gemabox
  manifest.txt
  resources/
```

Isi manifest minimal:

```
name=gemabox
version=0.1
exec=gemabox
type=executable
depends=libgemapi>=0.1
depends=libgemrt>=0.1
capability=console
capability=filesystem.read
```

Format manifest sengaja sederhana `key=value` agar belum membutuhkan parser
JSON. Format terstruktur bisa menyusul.

Dependency library diselesaikan dari `/System/Libraries`. Jika aplikasi
membutuhkan library tambahan, library itu juga harus ada di volume
filesystem yang sudah dipasang.

## 6. PATH dan command

Usulan PATH:

```
/Applications
/System/Commands
/Commands
```

Aturan command:

- `gemabox` adalah multi-call binary. Perintah `ls`, `cat`, `echo` bisa
  menjadi applet di dalamnya.
- Aplikasi lain adalah binary terpisah di `/Applications`.
- `/Commands` berisi link atau tabel applet, bukan binary duplikat.
- Shell mencari command di PATH dan memanggil `spawn` atau `exec`.

## 7. Permission dan atribut

Minimal yang dibutuhkan:

- Read-only untuk system dan aplikasi.
- Read-write untuk data user.
- Executable flag.
- Pemilik dan mode nanti.
- Mount read-only harus ditolak jika ada operasi tulis.

FAT32 tidak punya permission Unix dan symlink. Untuk system volume jangka
panjang, ext2 atau filesystem read-only khusus lebih cocok. FAT32 bisa
dipakai untuk volume data dan pertukaran file.

## 8. Boot image dan ESP

Usulan pemisahan:

- ESP berisi hanya `EFI/BOOT/BOOTAA64.EFI`.
- Volume `System` berisi init, library, service, dan aplikasi inti.
- Volume `Data` berisi konfigurasi dan berkas user.
- Kernel tidak menanam gemabox atau library.

## 9. Yang masih dibahas

- Nama `/Applications` versus `/Apps` atau `/Programs`.
- Apakah `/System/Commands` diperlukan.
- Apakah system volume memakai FAT32 dulu atau langsung ext2.
- Apakah package manager berada di init atau service terpisah.
- Apakah path case-sensitive seperti Linux atau case-insensitive seperti
  Windows.

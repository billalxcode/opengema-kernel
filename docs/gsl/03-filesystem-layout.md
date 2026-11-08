# 03 - Layout Filesystem

Status: GSLFS, system image, root read-only, writable ramfs di `/etc`,
`/users`, `/var`, `/tmp`, dan init PID 1 sudah berjalan (GSL-3 dan awal
GSL-4). Volume persisten, service manager, dan recovery menyusul.

Layout memakai satu root Unix-like dengan path lowercase. System dan
aplikasi read-only. Data pengguna, konfigurasi, state, dan temp writable.

## 1. Prinsip

1. Satu root, tanpa drive letter.
2. Path case-sensitive seperti Unix.
3. System dan aplikasi tidak bisa ditulis saat runtime.
4. Home pengguna, `/etc`, `/var`, dan `/tmp` writable.
5. `/dev` dan `/proc` adalah filesystem virtual dari kernel.
6. Yang di-mount adalah volume, bukan setiap library.
7. Setiap path punya pemilik, mode, dan capability.
8. Library hanya dimuat dari `/system/libraries`.

### 1.1 Sifat mutlak

Layout ini adalah bagian dari ABI GSL, bukan konfigurasi pengguna. Sumber
tunggal konstanta adalah `packages/libgsl/include/gsl/layout.h`.

- Layout ditentukan saat system image dibuat dan tidak berubah saat runtime.
- User, aplikasi, dan service tidak boleh menambah, menghapus, memindahkan,
  atau mengganti nama mount point sistem.
- `gsl.conf`, environment, dan manifest package tidak boleh mengubah layout.
- `init` memakai tabel mount tetap. Direktori wajib yang hilang di image
  adalah boot error, bukan kesempatan membuat path baru.
- Volume tambahan hanya boleh dipasang di `/volumes/<name>`.
- Aplikasi menyimpan data pribadi di `/users/<user>` atau file sementara di
  `/tmp`, bukan membuat direktori top-level baru.
- Perubahan layout memerlukan system image dan versi ABI baru, bukan
  konfigurasi user.

## 2. Pohon final

```
/
  system/                   read-only, inti sistem
    boot/
      init                  proses pertama dari kernel
      ld-gem                dynamic linker statis
    libraries/
      libkgema.so.1
      libgemapi.so.1
    services/
      service-manager
      device-manager
    config/                 default read-only
      gsl.conf
      version
      users
    commands/               tool internal, tidak selalu di PATH
  applications/             read-only, package aplikasi
    gemabox/
      gemabox               ELF executable
      manifest.txt
      resources/
    editor/
    files/
  commands/                 read-only, link atau tabel command
    gemabox
    ls
    cat
    echo
  etc/                      writable, konfigurasi lokal
    gsl.conf
    hostname
  users/                    writable, home per pengguna
    <user>/
      .config/
      Documents/
      Downloads/
  tmp/                      writable, volatile, dibersihkan saat boot
  var/                      writable, state sistem
    log/
    run/
    cache/
    spool/
  dev/                      devfs virtual
    console
    null
    random
    blk0
  proc/                     procfs virtual
    <pid>/
      cmdline
      status
      fd/
  volumes/                  mount point volume tambahan
    <name>/
```

Tidak ada `/home`. Home pengguna berada di `/users/<user>`.

## 3. Tabel mount

| Mount | Sumber | Mode | Keterangan |
|---|---|---|---|
| `/` | volume system GSLFS | read-only | berisi `/system`, `/applications`, `/commands`, `/users`, `/etc`, `/var`, `/tmp`, `/volumes` sebagai direktori kosong |
| `/etc` | volume config | read-write | konfigurasi lokal |
| `/users` | volume home | read-write | home pengguna |
| `/var` | volume state | read-write | log, run, cache, spool |
| `/tmp` | tmpfs atau ramfs | read-write | volatile, dibersihkan saat boot |
| `/dev` | devfs | virtual | device node |
| `/proc` | procfs | virtual | proses dan status kernel |
| `/volumes/<name>` | volume tambahan | sesuai volume | data, USB, disk kedua |

Direktori mount point harus ada di image system sebagai direktori kosong.
Mount menimpa isi direktori itu, bukan menghapusnya.

## 4. Read-only dan writable

| Path | Akses | Pemilik | Mode | Isi |
|---|---|---|---|---|
| `/system` | read-only | root | 0755 | OS, library, service, config default |
| `/applications` | read-only | root | 0755 | package aplikasi |
| `/commands` | read-only | root | 0755 | link atau tabel command |
| `/etc` | read-write | root | 0755 | konfigurasi lokal |
| `/users/<user>` | read-write | user | 0700 | home pribadi |
| `/tmp` | read-write | root | 1777 | file sementara, sticky |
| `/var` | read-write | root | 0755 | state sistem |
| `/dev` | virtual | kernel | 0755 | device node |
| `/proc` | virtual | kernel | 0555 | informasi proses |
| `/volumes` | mount point | root | 0755 | volume tambahan |

Aturan:

- Operasi tulis ke mount read-only ditolak VFS, bukan hanya oleh aplikasi.
- `/users/<user>` hanya bisa ditulis pemilik atau root.
- `/tmp` memakai sticky bit agar pengguna tidak menghapus file pengguna lain.
- File system read-only tidak boleh punya hard link ke volume writable.
- Setiap executable harus punya bit executable.

Ringkasan akses:

```
Read-only untuk semua   /system  /applications  /commands
Writable pengguna       /users/<user>  /tmp
Writable admin/service  /etc  /var
Virtual kernel          /dev  /proc
Mount point             /volumes
```

Pengguna biasa tidak menulis `/etc` atau `/var`. Hanya root dan service
yang memiliki capability terkait yang boleh menulis path itu.

## 5. Model permission

- UID 0 untuk root, UID 1000 ke atas untuk pengguna.
- Mode bit Unix: read, write, execute untuk owner, group, other.
- Setuid dan setgid ditunda sampai ada model credential yang matang.
- Permission path dipakai untuk filesystem.
- Capability dipakai untuk device dan operasi sensitif, misalnya
  `device.blk0.read`, `system.mount`, dan `process.spawn`.
- Capability tidak bisa diganti dengan path string mentah.
- Root di kernel bukan pengguna biasa. Aplikasi tidak pernah mendapat
  akses hardware langsung.

Implementasi saat ini belum punya UID/GID per proses. Mode dan ownership
disimpan di GSLFS dan baru ditegakkan setelah proses punya credential
(GSL-6).

## 6. Library dan executable

- Shared object hanya dimuat dari `/system/libraries`.
- `ld-gem` tidak membaca `LD_LIBRARY_PATH` dari environment.
- Executable aplikasi berada di `/applications/<package>/`.
- `/commands` berisi link, launcher, atau tabel applet. Bukan salinan
  binary.
- `init` dan `ld-gem` berada di `/system/boot/`.
- Library milik package tidak dimuat dari path package tanpa kebijakan
  eksplisit. Untuk sekarang semua library sistem berada di
  `/system/libraries`.

## 7. Package dan manifest

```
/applications/gemabox/
  gemabox
  manifest.txt
  resources/
```

Manifest minimal:

```
name=gemabox
version=0.1
exec=gemabox
type=executable
depends=libgemapi>=0.1
capability=console
capability=filesystem.read:/system
capability=filesystem.write:/users/<user>
```

Command:

- `gemabox` adalah multi-call binary untuk applet inti.
- Aplikasi besar adalah binary terpisah di `/applications`.
- `/commands/gemabox` menunjuk ke `/bin/gemabox`.
- `/commands/ls`, `/commands/cat`, dan `/commands/echo` bisa menunjuk ke
  binary yang sama dengan nama applet.

PATH:

```
/commands
```

Shell mencari command di `/commands`. Direktori package tidak masuk PATH
secara langsung.

## 8. Boot dan urutan mount

```
kernel
  -> mount volume system GSLFS read-only di /
  -> mount devfs di /dev dan procfs di /proc
  -> mount writable ramfs di /etc, /users, /var, /tmp
  -> mount ESP FAT32 di /volumes/boot
  -> load /system/boot/init
  -> PID 1 user space
       -> jalankan service-manager (nanti)
       -> jalankan shell
```

Untuk sementara kernel memasang ramfs writable di path tetap. Setelah
syscall mount user space tersedia, init yang memasang volume writable
dengan tabel tetap yang sama. User tidak dapat mengubah urutan atau path.

Jika volume writable persisten belum ada atau rusak:

- `/etc` memakai default dari `/system/config`.
- `/users` dan `/var` memakai ramfs sementara.
- Shell recovery tetap bisa dijalankan.

## 9. Status implementasi

Layout yang sudah berjalan:

```
/                       GSLFS read-only, partisi kedua
  system/boot/init
  system/boot/ld-gem
  system/libraries/libkgema.so.1
  system/libraries/libgemapi.so.1
  system/config/version
  system/config/gsl.conf
  system/packages/registry
  bin/gemabox
  applications/gemabox/manifest.txt
  commands/
  etc/                  direktori kosong read-only di image
  users/
  tmp/
  var/
  dev/
  proc/
  volumes/boot/
/etc                    ramfs writable sementara
/users                  ramfs writable sementara
/var                    ramfs writable sementara
/tmp                    ramfs writable sementara
/dev                    devfs
/proc                   procfs
/volumes/boot           FAT32 ESP
```

Yang belum:

- Volume writable persisten untuk `/etc`, `/users`, dan `/var`.
- Service manager dan recovery.
- PATH, package manager, dan command lookup.
- Penegakan mode, UID, dan GID.

## 10. Keputusan

| Topik | Keputusan |
|---|---|
| Gaya path | Lowercase Unix |
| Home | `/users/<user>` |
| Aplikasi | `/applications/<package>` |
| Command | `/commands`, masuk PATH |
| Konfigurasi | `/etc` writable, default di `/system/config` |
| Device | `/dev` |
| Proses | `/proc` |
| Volume tambahan | `/volumes/<name>` |
| Temp | `/tmp`, volatile |
| State sistem | `/var` |
| Library | `/system/libraries` |
| Read-only | Mount read-only, mode bit, capability |
| Case sensitivity | Case-sensitive |

## 11. Yang masih dibahas

- Filesystem writable untuk `/etc`, `/users`, dan `/var`: GSLFS-RW,
  ext2, atau FAT32 untuk volume pertukaran.
- Apakah `/etc` berupa volume penuh atau overlay dari `/system/config`.
- Apakah `/var` dan `/users` berada di volume yang sama.
- Apakah `/tmp` memakai ramfs murni atau disk dengan pembersihan saat boot.
- Dukungan symlink di GSLFS untuk `/commands`.
- Apakah `/usr` diperlukan untuk kompatibilitas tool Unix.
- Alokasi UID dan mekanisme login pengguna.

# 06 - Roadmap GSL

Roadmap ini adalah urutan kerja untuk mengubah userland dari satu binary
yang ditanam kernel menjadi sistem package dan API berlapis dengan dynamic
library.

Keputusan brainstorming:

- Aplikasi memakai dynamic library langsung.
- Satu volume system read-only dengan GSLFS.
- Kernel memuat executable dan `PT_INTERP`.
- Dynamic linker `ld-gem` berjalan di user space.
- Init berjalan di user space.
- gemabox hybrid: applet inti dan aplikasi terpisah.
- Manifest `key=value`.
- Capability hybrid path dan handle.

## GSL-0 - Brainstorm dan keputusan

Status: selesai untuk keputusan utama.

- [x] Definisikan lima layer.
- [x] Definisikan pembagian libkgema dan libgemapi.
- [x] Pilih dynamic library.
- [x] Pilih satu volume system GSLFS.
- [x] Pilih kernel exec dan user dynamic linker.
- [x] Pilih user init.
- [x] Pilih gemabox hybrid.
- [x] Pilih manifest `key=value`.
- [x] Pilih capability hybrid.
- [ ] Detail format GSLFS dan manifest.
- [ ] Detail capability per resource.

## GSL-1 - Pisahkan library dan ABI

Tujuan: aplikasi hanya melihat API, dan library dibangun sebagai shared
object.

- [x] Buat `packages/libgemapi`.
- [x] Pindahkan `libkgema` menjadi dependency privat.
- [x] Build `libkgema.so` dan `libgemapi.so`.
- [x] Tetapkan soname `libkgema.so.1` dan `libgemapi.so.1`.
- [x] `gemabox` hanya memakai header `gemapi/`.
- [x] Tambah `scripts/check-gsl.sh`.
- [ ] Symbol versioning dan aturan kompatibilitas ABI.
- [x] Siapkan `ld-gem` statis sebagai bootstrap.

Selesai bila: `libgemapi.so` dan `libkgema.so` bisa dibangun, dan aplikasi
tidak memiliki simbol atau include `kgema`.

Status: boundary API dan shared object sudah ada. `libgemapi.a` adalah satu
kesatuan link berisi objek API, `_start`, dan objek `libkgema` privat.
`libgemapi.so.1` sudah memiliki `DT_NEEDED` ke `libkgema.so.1` dan dimuat
oleh `ld-gem`. Symbol versioning menyusul.

## GSL-2 - ELF loader dan dynamic linker

Tujuan: binary dan dependency benar-benar dimuat dari filesystem.

- [x] ELF64 loader di kernel untuk ET_EXEC statis.
- [x] Dukungan `PT_LOAD`.
- [x] Dukungan `PT_INTERP` dan pemetaan interpreter.
- [x] Validasi header, mesin, entry, segmen, alignment, dan batas file.
- [x] Kernel memakai entry point dari ELF, bukan alamat tetap.
- [x] Binary uji tidak lagi berupa flat binary.
- [x] Test menolak file yang bukan ELF.
- [x] `ld-gem` memproses `DT_NEEDED` secara rekursif.
- [x] Relokasi `RELATIVE`, `GLOB_DAT`, `JUMP_SLOT`, dan `ABS64`.
- [x] Symbol binding dengan hash SYSV.
- [~] Search path `/ram` untuk bootstrap, target `/system/libraries`.
- [ ] Read-only code mapping dan W^X user.
- [ ] Test library hilang, versi salah, dan ELF rusak lanjutan.

Selesai bila: aplikasi dynamic berjalan dengan `libgemapi.so` dan
`libkgema.so` dari filesystem.

Status: kernel memuat `/ram/gemabox` sebagai ELF64, membaca `PT_INTERP`,
memetakan executable dan `/ram/ld-gem` dalam satu address space, lalu
menyerahkan `GEM_DYNAMIC` dan `GEM_ENTRY` lewat environment. `ld-gem`
memuat dependency dari ramfs, menerapkan relokasi, dan melompat ke entry
utama. Binary masih berasal dari ramfs, bukan GSLFS.

## GSL-3 - GSLFS dan system volume

Tujuan: system volume read-only dan reproducible.

- [x] Format GSLFS di `08-gslfs-format.md`.
- [x] Tool `mkgslimg`.
- [x] Driver GSLFS read-only di kernel.
- [~] Image system berisi `ld-gem`, library, dan aplikasi inti. `init`
  menyusul pada GSL-4.
- [x] Mount `/` sebagai volume system read-only.
- [~] `/etc`, `/users`, dan `/var` writable belum ada. `/tmp` sementara
  memakai ramfs.
- [x] Mount `/dev` dan `/proc` sebagai filesystem virtual.
- [x] Validasi checksum dan offset.
- [x] Build image deterministik.

Selesai bila: mengganti isi image system mengubah program yang dijalankan
tanpa rebuild kernel.

Status: image GSLFS dibuat dari `packages/system-root` dan ditulis ke
partisi kedua. Kernel memvalidasi checksum, me-mount root read-only,
memuat `/applications/gemabox/gemabox` dan `/system/boot/ld-gem`, lalu
`ld-gem` memuat library dari `/system/libraries`. ESP FAT32 di-mount di
`/volumes/boot`. Volume writable `/etc`, `/users`, dan `/var` serta init
user belum ada.

## GSL-4 - Init dan shell

Tujuan: kernel menyerahkan kebijakan ke user space.

- [x] `/system/boot/init` sebagai PID 1.
- [x] Init memasang layout tetap; user tidak dapat mengubah mount point.
- [~] `/etc`, `/users`, `/var`, dan `/tmp` writable memakai ramfs yang
  dipasang kernel. Syscall mount user space menyusul.
- [x] Shell dijalankan oleh init.
- [~] Service manager minimal belum ada; init langsung menjalankan shell.
- [ ] Recovery shell jika init gagal.
- [ ] Shutdown dan reboot lewat service manager.

Selesai bila: kernel hanya memuat init, lalu init memasang volume dan
menjalankan shell.

Status: kernel memuat `/system/boot/init`, lalu init men-spawn
`/applications/gemabox/gemabox shell` dan menunggu. `/etc`, `/users`,
`/var`, dan `/tmp` sudah writable lewat ramfs pada path tetap. Volume
persisten, service manager, dan recovery belum ada.

## GSL-5 - Package dan command

- [ ] Manifest `key=value`.
- [ ] Dependency resolver.
- [ ] PATH dan command lookup.
- [ ] gemabox multi-call applet.
- [ ] Aplikasi eksternal di `/applications`.
- [ ] Install dan update package.
- [ ] `ls`, `ps`, `sleep`, dan `clear` di gemabox.

Selesai bila: shell bisa menjalankan command internal dan aplikasi eksternal
hanya dari filesystem.

## GSL-6 - Capability dan sandbox

- [ ] Capability di manifest.
- [ ] Permission path untuk filesystem.
- [ ] Handle untuk device dan operasi sensitif.
- [ ] Pemeriksaan di libgemapi.
- [ ] Pemeriksaan di kernel.
- [ ] Batas memori, thread, dan FD.
- [ ] Penolakan akses hardware langsung.
- [ ] Test keamanan dan fuzzing.

Selesai bila: aplikasi tanpa capability tidak bisa membuka console, file,
atau device yang dilindungi.

## GSL-7 - Service dan recovery

- [ ] Service manager user space.
- [ ] Restart service yang crash.
- [ ] Log service.
- [ ] Recovery mode.
- [ ] Update system volume A/B.
- [ ] Rollback jika image baru gagal validasi.

Selesai bila: shell tetap hidup saat aplikasi atau service crash, dan system
volume bisa diperbarui tanpa membongkar kernel.

## Urutan yang disarankan

1. GSL-1: boundary API dan shared object.
2. GSL-2: ELF loader dan `ld-gem`.
3. GSL-3: GSLFS dan system volume.
4. GSL-4: init dan shell.
5. GSL-5: package dan command.
6. GSL-6: capability.
7. GSL-7: service dan recovery.

## Bootstrap

Selama GSL-1 sampai GSL-3 belum selesai, gemabox statis yang sekarang tetap
dipakai sebagai program uji. Itu bukan target akhir, hanya bootstrap.

Program statis yang diizinkan jangka panjang:

- `ld-gem` sebagai dynamic linker.
- Recovery tool minimal jika diperlukan.

## Hubungan dengan roadmap kernel

- ELF loader bergantung pada VFS dan `mmap` file.
- Dynamic linker bergantung pada relokasi, shared object, dan W^X user.
- GSLFS membutuhkan driver block dan VFS read-only.
- Init bergantung pada `spawn`, `wait`, dan filesystem writable.
- Capability bergantung pada process object dan syscall validation.
- Service manager bergantung pada lifecycle proses dan logging.

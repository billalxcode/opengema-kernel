# 06 - Roadmap GSL

Roadmap ini adalah urutan kerja untuk mengubah userland dari satu binary
yang ditanam kernel menjadi sistem package dan API berlapis.

## GSL-0 - Brainstorm dan keputusan

Status: dokumen ini.

- [x] Definisikan lima layer.
- [x] Definisikan pembagian libkgema dan libgemapi.
- [x] Usulkan layout filesystem.
- [x] Usulkan alur boot dan exec.
- [ ] Putuskan static versus dynamic untuk tahap pertama.
- [ ] Putuskan filesystem system volume.
- [ ] Putuskan format manifest dan capability.

## GSL-1 - Pisahkan library

Tujuan: aplikasi hanya melihat API.

- [ ] Buat `packages/libgemapi` dengan header publik.
- [ ] Pindahkan `libkgema` menjadi dependency privat.
- [ ] `libgemapi` membungkus `libkgema`.
- [ ] `gemabox` hanya memakai `libgemapi`.
- [ ] Tambah `scripts/check-gsl.sh`.
- [ ] Build memastikan aplikasi tidak menautkan `libkgema`.

Selesai bila: gemabox berjalan tanpa include atau simbol `kgema` di source
aplikasi.

## GSL-2 - ELF dan binary dari filesystem

Tujuan: binary tidak lagi ditanam di kernel.

- [ ] Loader ELF64 di kernel.
- [ ] Program pertama dibaca dari `/System/Boot/init`.
- [ ] gemabox dibaca dari `/Applications/gemabox/gemabox`.
- [ ] Kernel tidak lagi menyalin `GEMABOX.BIN` ke ramfs.
- [ ] Validasi segment, entry, dan stack.
- [ ] Test binary hilang, ELF rusak, dan entry salah.

Selesai bila: mengganti binary di volume system mengubah program yang
dijalankan tanpa rebuild kernel.

## GSL-3 - System volume dan init

Tujuan: kernel menyerahkan kebijakan ke user space.

- [ ] Volume system read-only.
- [ ] Init sebagai proses PID 1.
- [ ] Mount `/System`, `/Applications`, dan `/Data`.
- [ ] Service manager minimal.
- [ ] Shell dijalankan oleh init.
- [ ] Recovery shell jika init gagal.

Selesai bila: kernel hanya memuat init, lalu init memasang volume dan
menjalankan shell.

## GSL-4 - Dynamic library

Tujuan: dependency library benar-benar berasal dari filesystem.

- [ ] Shared object ELF.
- [ ] `DT_NEEDED` dan symbol resolution.
- [ ] Relokasi dasar.
- [ ] Dynamic linker membaca `/System/Libraries`.
- [ ] Versi library.
- [ ] Read-only code mapping dan W^X user.
- [ ] Test library hilang, versi salah, dan simbol tidak ditemukan.

Selesai bila: aplikasi menautkan `libgemapi.so` dan `libkgema.so` dari
system volume, bukan dari binary aplikasi.

## GSL-5 - Package dan command

- [ ] Manifest package.
- [ ] Dependency resolver.
- [ ] PATH dan command lookup.
- [ ] gemabox multi-call applet.
- [ ] Aplikasi eksternal di `/Applications`.
- [ ] Install dan update package.

Selesai bila: shell bisa menjalankan command internal dan aplikasi eksternal
hanya dari filesystem.

## GSL-6 - Capability dan sandbox

- [ ] Capability di manifest.
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
- [ ] Update system volume dengan rollback.

Selesai bila: shell tetap hidup saat aplikasi atau service crash, dan system
volume bisa diperbarui tanpa membongkar kernel.

## Urutan yang disarankan

1. GSL-1: boundary library.
2. GSL-2: ELF dan file-based binary.
3. GSL-3: init dan volume system.
4. GSL-4: dynamic library.
5. GSL-5: package dan command.
6. GSL-6: capability.
7. GSL-7: service dan recovery.

## Hubungan dengan roadmap kernel

- ELF loader bergantung pada VFS dan `mmap` file.
- Dynamic linker bergantung pada shared memory, relokasi, dan W^X user.
- Init bergantung pada `spawn`, `wait`, dan filesystem writable.
- Capability bergantung pada process object dan syscall validation.
- Service manager bergantung pada thread/process lifecycle dan logging.

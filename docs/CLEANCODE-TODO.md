# Clean Code Refactor Plan - dev

Branch: `dev`.
Skill: `.opencode/skills/opengema-cleancode/`.
Baseline commit: `12289ce`.
Tanggal: 2026-11-01.

## Batasan verifikasi

Podman, QEMU, dan cross linker tidak tersedia setelah restart server. Karena
itu setiap grup diverifikasi dengan:

```sh
python3 scripts/compile-check.py
```

Script ini memakai `build/compile_commands.json` dari build terakhir dan
menjalankan `clang++ -fsyntax-only` untuk setiap file. Full `test.sh` harus
dijalankan di environment build sebelum branch `dev` di-merge.

## Inventaris

Ukuran file terbesar:

```
788 kernel/mem/vmm.cpp
673 kernel/sched/scheduler.cpp
598 kernel/process/process.cpp
596 kernel/syscall/syscall.cpp
566 kernel/fs/fat32.cpp
512 kernel/drivers/virtio/virtio_common.cpp
424 kernel/fs/gslfs.cpp
416 kernel/platform/fdt.cpp
386 kernel/drivers/virtio/virtio_gpu.cpp
350 kernel/core/init/filesystem.cpp
344 kernel/fs/vfs.cpp
333 kernel/core/init/self_test.cpp
331 packages/ld-gem/src/linker.cpp
324 kernel/io/framebuffer.cpp
```

Temuan utama:

- Duplikasi little-endian reader di `efi_image.cpp`, `gslfs.cpp`,
  `partition.cpp`, dan `elf.cpp`.
- Duplikasi helper teks di `ld-gem` (`main.cpp`, `linker.cpp`,
  `image.cpp`).
- `gslfs::Mount` menangani superblock, node, name, tree, dan checksum
  sekaligus.
- `SetupWritableFs` menangani tree, mount, seed config, dan test tulis.
- `process::SpawnWithArgs` menangani baca ELF, map, stack, dan process
  object.
- `syscall.cpp` mencampur dispatch, path copy, TTY, dan semua handler.
- `vmm.cpp` dan `scheduler.cpp` besar, tetapi berisiko dipecah tanpa QEMU.
- Test GSLFS, syscall list, dan error path masih kurang.
- Lock ordering belum didokumentasikan.

## Grup 0 - Baseline

- [x] Buat branch `dev`.
- [x] Tambah `scripts/compile-check.py`.
- [x] Baseline compile-check 90 file OK.
- [x] Tulis rencana ini.

Commit: `chore(dev): add the clean code refactor plan`.

## Grup 1 - Shared primitives

Tujuan: hapus duplikasi kecil yang aman.

- [ ] Tambah `libs/core/endian.h`.
- [ ] Ganti `Read16/32/64` duplikat di `efi_image.cpp`.
- [ ] Ganti `Read32/64` duplikat di `gslfs.cpp`.
- [ ] Ganti `Read32/64` duplikat di `partition.cpp`.
- [ ] Ganti reader span di `elf.cpp` dengan helper bersama.
- [ ] Tambah `ldgem/text.h` dan `text.cpp`.
- [ ] Pakai helper teks di `ld-gem/main.cpp`.
- [ ] Pakai helper teks di `ld-gem/linker.cpp`.
- [ ] Compile-check grup.

Commit: `refactor(core): share endian and linker text helpers`.

## Grup 2 - Kernel filesystem

Tujuan: fungsi kecil, satu tanggung jawab.

- [ ] Pecah `gslfs::Mount` menjadi `ReadSuperblock`, `LoadNodes`,
  `ValidateTree`, dan `VerifyData`.
- [ ] Pecah `SetupWritableFs` menjadi `CreateWritableTree`,
  `MountWritableTree`, `SeedEtcConfig`, dan `VerifyTmpWrite`.
- [ ] Pisahkan FD table dari path resolution di `vfs.cpp` jika aman.
- [ ] Tambah test host GSLFS dari `mkgslimg`.
- [ ] Compile-check grup.

Commit: `refactor(fs): split gslfs mount and writable setup`.

## Grup 3 - Process dan syscall

Tujuan: batas tanggung jawab yang jelas.

- [ ] Pindahkan load ELF dari `process.cpp` ke helper `elf.cpp`.
- [ ] Pisahkan `SpawnWithArgs` menjadi `LoadProgram`, `CreateAddressSpace`,
  `CreateProcess`.
- [ ] Pisahkan handler syscall per kategori: io, fs, process.
- [ ] Tambah test error `list` dan `spawn`.
- [ ] Compile-check grup.

Commit: `refactor(process): split program loading and syscall handlers`.

## Grup 4 - VMM dan scheduler

Tujuan: modul besar dipecah tanpa mengubah perilaku.

- [ ] Pisahkan address space init/destroy dari mapping.
- [ ] Pisahkan copy/query dari map/unmap.
- [ ] Pisahkan thread creation, run queue, dan wait queue.
- [ ] Dokumentasikan lock ordering.
- [ ] Compile-check grup.

Commit: `refactor(mem): split vmm and scheduler responsibilities`.

Grup ini ditunda sampai full QEMU test tersedia.

## Grup 5 - Driver dan platform

- [ ] Pisahkan `virtio_common.cpp` menjadi queue dan transport.
- [ ] Pisahkan `fdt.cpp` menjadi tokenizer dan property access.
- [ ] Pisahkan `framebuffer.cpp` menjadi font, drawing, dan console.
- [ ] Compile-check grup.

Commit: `refactor(drivers): split virtio fdt and framebuffer helpers`.

## Grup 6 - Userland

- [ ] Pisahkan `ld-gem/linker.cpp` menjadi dynamic parse dan relocation.
- [ ] Pisahkan applet gemabox dari shell loop jika perlu.
- [ ] Tambah host test path helper.
- [ ] Compile-check grup.

Commit: `refactor(userland): split linker and applet helpers`.

## Grup 7 - Test dan dokumentasi

- [ ] Tambah test image GSLFS valid dan rusak.
- [ ] Tambah test negative syscall.
- [ ] Update `docs/CLEANCODE-REVIEW.md`.
- [ ] Jalankan `test.sh` di environment build.
- [ ] Merge checklist.

Commit: `test(dev): add gslfs and syscall negative tests`.

## Aturan commit

- Satu grup satu commit.
- Jangan campur refactor dengan fitur.
- Compile-check hijau sebelum commit.
- Catat risiko dan verifikasi yang belum dijalankan.

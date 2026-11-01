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

- [x] Tambah `libs/core/endian.h`.
- [x] Ganti `Read16/32/64` duplikat di `efi_image.cpp`.
- [x] Ganti `Read32/64` duplikat di `gslfs.cpp`.
- [x] Ganti `Read32/64` duplikat di `partition.cpp`.
- [x] Ganti reader span di `elf.cpp` dengan helper bersama.
- [x] Tambah `ldgem/text.h` dan `text.cpp`.
- [x] Pakai helper teks di `ld-gem/main.cpp`.
- [x] Pakai helper teks di `ld-gem/linker.cpp`.
- [x] Compile-check grup.

Commit: `refactor(core): share endian and linker text helpers`.

## Grup 2 - Kernel filesystem

Tujuan: fungsi kecil, satu tanggung jawab.

- [x] Pecah `gslfs::Mount` menjadi `ReadSuperblock`, `LoadNodes`,
  `ValidateTree`, dan `VerifyNodeData`.
- [x] Pecah `SetupWritableFs` menjadi `CreateWritableTree`,
  `MountWritableTree`, `SeedEtcConfig`, dan `VerifyTmpWrite`.
- [x] Pisahkan mount lookup dan path walk di `vfs.cpp` (`FindBestMount`,
  `ResolveSegments`).
- [ ] Pisahkan FD table dari path resolution di `vfs.cpp` jika aman.
- [x] Tambah test host GSLFS dari `mkgslimg`.
- [x] Tambah test host driver GSLFS (`Mount`, lookup, read, checksum).
- [x] Tambah test host ramfs (`Create`, `CreateDir`, `CreateFile`, read, write, iterate).
- [x] Tambah test host VFS untuk mount, path, dan `..`.
- [x] Compile-check grup.

Catatan: pemisahan FD table di `vfs.cpp` ditunda karena menyentuh jalur
syscall dan lebih aman setelah QEMU tersedia.

Commit: `refactor(fs): split gslfs mount and writable setup`.

## Grup 3 - Process dan syscall

Tujuan: batas tanggung jawab yang jelas.

- [x] Ekstrak `BuildInterpreterEnvironment` dari `SpawnWithArgs`.
- [x] Tambah host test ELF parser (`Parse` valid dan invalid).
- [ ] Pindahkan load ELF dari `process.cpp` ke helper `elf.cpp`.
- [ ] Pisahkan `SpawnWithArgs` menjadi `LoadProgram`, `CreateAddressSpace`,
  `CreateProcess`.
- [ ] Pisahkan handler syscall per kategori: io, fs, process.
- [ ] Tambah test error `list` dan `spawn`.
- [ ] Compile-check grup.

Catatan: pemisahan load ELF dan syscall ditunda sampai QEMU tersedia.
Ekstraksi environment sudah mengurangi state lokal di `SpawnWithArgs`.

Commit: `refactor(process): extract interpreter environment`.

## Grup 4 - VMM dan scheduler

Tujuan: modul besar dipecah tanpa mengubah perilaku.

- [x] Perbaiki formatting `VmmMapDevice`.
- [x] Dokumentasikan lock ordering di `docs/LOCKING.md`.
- [ ] Pisahkan address space init/destroy dari mapping.
- [ ] Pisahkan copy/query dari map/unmap.
- [ ] Pisahkan thread creation, run queue, dan wait queue.
- [ ] Compile-check grup.

Status: dokumentasi dan formatting selesai. Pemisahan file ditunda karena
menyentuh MMU, TLB, dan context switch. Tanpa QEMU, risiko regresi terlalu
tinggi.

Commit: `docs(mem): document kernel lock ordering`.

## Grup 5 - Driver dan platform

- [ ] Pisahkan `virtio_common.cpp` menjadi queue dan transport.
- [ ] Pisahkan `fdt.cpp` menjadi tokenizer dan property access.
- [ ] Pisahkan `framebuffer.cpp` menjadi font, drawing, dan console.
- [x] Tambah host test FDT (`FindNode`, `FindProperty`, `Child`, `NodeName`).
- [ ] Compile-check grup.

Status: test FDT selesai. Pemisahan file ditunda karena perubahan driver
tidak bisa diverifikasi tanpa boot QEMU.

Commit: `test(dev): add fdt host test`.

## Grup 6 - Userland

- [x] Pisahkan helper teks `ld-gem` ke `ldgem/text`.
- [ ] Pisahkan `ld-gem/linker.cpp` menjadi dynamic parse dan relocation.
- [ ] Pisahkan applet gemabox dari shell loop jika perlu.
- [x] Tambah host test endian.
- [ ] Compile-check grup.

Status: refactor kecil selesai. Pemisahan linker ditunda karena perubahan
tersebut menyentuh ABI dynamic linking dan lebih baik diuji dengan QEMU.

Commit: `refactor(core): share endian and linker text helpers`.

## Grup 7 - Test dan dokumentasi

- [x] Tambah test image GSLFS valid dan rusak.
- [x] Tambah test host untuk `endian.h`.
- [x] Tambah test host GPT/partition (`FindEspPartition`, `FindPartitionByMagic`).
- [x] Tambah test host VFS (`FsMount`, `FsResolve`, `FsStat`, path `..`).
- [x] Tambah test host ELF parser valid dan invalid.
- [x] Tambah test negative `ls /missing` di test QEMU (menunggu QEMU).
- [ ] Update `docs/CLEANCODE-REVIEW.md`.
- [ ] Jalankan `test.sh` di environment build.
- [ ] Merge checklist.

Commit: `test(dev): add gslfs endian and list error tests`.

## Status akhir sesi

| Grup | Status | Commit |
|---|---|---|
| 0 Baseline | selesai | `4400e7a` |
| 1 Shared primitives | selesai | `54ae45b` |
| 2 Filesystem | selesai | `14a325a` |
| 3 Process | sebagian | `7250ab5` |
| 4 VMM/scheduler | sebagian | `2f87675` |
| 5 Driver/platform | sebagian | `e6a9bfa` |
| 6 Userland | sebagian | `54ae45b` |
| 7 Test/dokumentasi | sebagian | `8c6a092` |

Verifikasi yang berjalan di sesi ini:

```
python3 scripts/compile-check.py
host tests: all passed
ramfs probe: OK
gslfs probe: OK
gslfs test: OK
```

Full `test.sh` belum berjalan karena podman dan QEMU tidak tersedia.

## Aturan commit

- Satu grup satu commit.
- Jangan campur refactor dengan fitur.
- Compile-check hijau sebelum commit.
- Catat risiko dan verifikasi yang belum dijalankan.

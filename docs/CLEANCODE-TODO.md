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
- [x] Pecah `InitFilesystem` menjadi `FindBlockDevice`, `InitFileTables`,
  `MountSystemVolume`, `ProbeLibrary`, `MountDevfs`,
  `MountOptionalFilesystems`, `ProbeConsole`, dan `ListRoot`.
- [x] Pisahkan mount lookup dan path walk di `vfs.cpp` (`FindBestMount`,
  `ResolveSegments`).
- [x] Pisahkan FD table dari path resolution di `vfs.cpp`
  (`fd_table.cpp` + `og/fs/vfs_internal.h`).
- [x] Satukan daftar device devfs dalam satu tabel untuk lookup dan
  iteration.
- [x] Pecah `FindFiltered` menjadi `ReadGptHeader`, `MatchesType`,
  `ParsePartition`, dan `CheckMagic`.
- [x] Pecah `LoadNodes` menjadi `LoadNode` per node.
- [x] Pecah `LoadNode` menjadi `ReadNodeRecord`, `ParseNodeFields`,
  `AssignNodeName`, dan `AssignNodeExtent`.
- [x] Pecah `ValidateTree` dengan `ReachesRoot` dan `DirIterate` dengan
  `FillDirEntry`.
- [x] Pecah `ReadSuperblock` menjadi `ParseSuperblockFields`,
  `CheckSuperblockHeader`, dan `CheckSuperblockLayout`.
- [x] Pecah `fat32::Mount` menjadi `ParseBootSector` dan pembuatan root.
- [x] Pecah `WalkDirectory` dengan `ParseEntry` untuk satu slot direktori.
- [x] Pecah `FileRead` FAT32 dengan `SeekCluster` dan `AdvanceCluster`.
- [x] Tambah test host GSLFS dari `mkgslimg`.
- [x] Tambah test host driver GSLFS (`Mount`, lookup, read, checksum).
- [x] Tambah test siklus parent GSLFS ditolak saat mount.
- [x] Tambah test host ramfs (`Create`, `CreateDir`, `CreateFile`, read, write, iterate).
- [x] Tambah test host VFS untuk mount, path, dan `..`.
- [x] Tambah test host VFS FD (`FsOpen`, `FsRead`, `FsWrite`, `FsSeek`,
  `FsClose`, descriptor reuse).
- [x] Compile-check grup.

Catatan: `og/fs/vfs_internal.h` hanya berisi `IsReady()` dan bukan header
publik. Semua header disimpan di tree `kernel/include/og`, bukan di
samping file `.cpp`. `fd_table.cpp` meng-include `og/fs/vfs.h` sebagai
deklarasi fungsi yang diimplementasikannya.

Commit: `refactor(fs): split gslfs mount and writable setup`.

## Grup 3 - Process dan syscall

Tujuan: batas tanggung jawab yang jelas.

- [x] Ekstrak `BuildInterpreterEnvironment` dari `SpawnWithArgs`.
- [x] Tambah host test ELF parser (`Parse` valid dan invalid).
- [x] Pindahkan load ELF dari `process.cpp` ke `loader.cpp`; `elf.cpp`
  tetap parser murni.
- [x] Ekstrak `LoadProgram` (termasuk `AddressSpaceInit` dan mapping) dari
  `SpawnWithArgs`.
- [x] Pisahkan alokasi dan pengisian process slot menjadi `CreateProcess`.
- [x] Pisahkan handler I/O (`DoRead`, `DoWrite`, `TtyReadLine`) ke
  `kernel/syscall/io.cpp`.
- [x] Pisahkan handler fs ke `kernel/syscall/fs.cpp` dan helper path ke
  `kernel/syscall/helpers.cpp`.
- [x] Pisahkan handler process (`DoGetPid`, `DoWait`, `DoSpawn`) ke
  `kernel/syscall/process.cpp`.
- [x] Pisahkan handler memory (`DoMmap`, `DoMunmap`) ke
  `kernel/syscall/memory.cpp`.
- [x] Tambah test error `list` dan `spawn` (lulus QEMU).
- [x] Compile-check `process.cpp` dan `loader.cpp`.

Catatan: `loader.cpp` menangani baca image, map segment, dan interpreter
path. Deklarasinya ada di `og/process/loader.h`. `LoadProgram` menghasilkan
`LoadedProgram`, lalu `CreateProcess` mengisi slot process. Deklarasi
handler syscall kategori ada di `og/syscall/syscall_internal.h`.
Semua handler syscall sudah dipisah per kategori; `syscall.cpp` hanya
berisi dispatch dan inisialisasi.

Commit: `refactor(process): extract interpreter environment`.

## Grup 4 - VMM dan scheduler

Tujuan: modul besar dipecah tanpa mengubah perilaku.

- [x] Perbaiki formatting `VmmMapDevice`.
- [x] Dokumentasikan lock ordering di `docs/LOCKING.md`.
- [x] Pisahkan copy user (`VmmCopyIn`, `VmmCopyOut`, `VmmUserContains`) ke
  `kernel/mem/vmm_user.cpp`.
- [x] Pisahkan query attribute dan translate ke `kernel/mem/vmm_query.cpp`.
- [x] Pisahkan address space init/destroy ke `kernel/mem/vmm_space.cpp`.
- [x] Pisahkan run queue ke `kernel/sched/run_queue.cpp`.
- [x] Pisahkan wait queue dan wakeup ke `kernel/sched/wait_queue.cpp`.
- [x] Pisahkan thread creation ke `kernel/sched/thread.cpp`.
- [x] Compile-check grup.

Status: VMM dan scheduler sudah dipisah per tanggung jawab dan
diverifikasi dengan QEMU.

Commit: `docs(mem): document kernel lock ordering`.

## Grup 5 - Driver dan platform

- [x] Pisahkan `Queue` dari `virtio_common.cpp` ke
  `kernel/drivers/virtio/virtio_queue.cpp`.
- [ ] Pisahkan `fdt.cpp` menjadi tokenizer dan property access.
- [x] Pisahkan `fdt.cpp` menjadi state/tokenizer (`fdt_state.cpp`) dan
  property access (`fdt_property.cpp`).
- [x] Pisahkan console framebuffer ke `kernel/io/framebuffer_console.cpp`.
- [x] Pisahkan font ke `kernel/io/font.cpp` dan drawing ke
  `kernel/io/framebuffer_draw.cpp`.
- [x] Tambah host test FDT (`FindNode`, `FindProperty`, `Child`, `NodeName`).
- [ ] Compile-check grup.

Status: virtio queue, framebuffer, dan FDT sudah dipisah dan diverifikasi
dengan host test serta QEMU.

Commit: `test(dev): add fdt host test`.

## Grup 6 - Userland

- [x] Pisahkan helper teks `ld-gem` ke `ldgem/text`.
- [x] Satukan mapping tipe node di `gemapi::fs::ConvertKind`.
- [x] Tambah host test libgemapi fs (`open`, `read`, `write`, `seek`,
  move, `stat`, `list`).
- [x] Tambah host test libkgema (`file`, `io`, `process`, `memory`,
  penerusan argumen syscall).
- [x] Tambah host test libgemapi io/process/memory (`read_line`, `spawn`,
  `wait`, `map`, `unmap`, `to_string`).
- [x] Pisahkan relocation `ld-gem` ke `packages/ld-gem/src/linker_reloc.cpp`.
- [x] Pisahkan dynamic parse `ld-gem` ke
  `packages/ld-gem/src/linker_dynamic.cpp`.
- [x] Pisahkan applet gemabox ke `shell.cpp` dan `writer.cpp`.
- [x] Tambah command registry di `shell.cpp` (`kCommands`), bukan
  rantai `if` hardcoded.
- [x] Dukung path langsung dan argumen untuk command eksternal.
- [x] Tambah host test endian.
- [x] Compile-check grup.

Status: relocation, dynamic parse, dan applet gemabox sudah dipisah
serta diverifikasi dengan QEMU. Command shell memakai registry tabel.

Commit: `refactor(core): share endian and linker text helpers`.

## Grup 7 - Test dan dokumentasi

- [x] Tambah test image GSLFS valid dan rusak.
- [x] Tambah test host untuk `endian.h`.
- [x] Tambah test host GPT/partition (`FindEspPartition`, `FindPartitionByMagic`).
- [x] Tambah test host GPT error path (signature rusak, count 0, disk
  pendek, tanpa reader, magic tidak dikenal).
- [x] Tambah test host VFS (`FsMount`, `FsResolve`, `FsStat`, path `..`).
- [x] Tambah test host VFS FD (`FsOpen`, `FsRead`, `FsWrite`, `FsSeek`,
  `FsClose`, descriptor reuse).
- [x] Tambah test host `FsList` dan error path (`nullptr`, mount
  non-directory, directory tanpa iterate).
- [x] Tambah test host procfs (`uptime`, `threads`, offset, iterate).
- [x] Tambah test host devfs (`console`, `null`, `zero`, lookup, iterate,
  read/write).
- [x] Tambah test host FAT32 (mount, lookup case-insensitive, offset read,
  nested directory, iterate, long name, error path dan validasi boot sector).
- [x] Tambah test host ELF parser valid dan invalid.
- [x] Tambah test negative `ls /missing` di test QEMU (lulus QEMU).
- [x] Tambah test negative spawn gagal di test QEMU (lulus QEMU).
- [x] Kernel build dan link lewat podman.
- [x] QEMU smoke test lewat container arm64.
- [ ] Update `docs/CLEANCODE-REVIEW.md`.
- [ ] Jalankan `test.sh` di environment build.
- [ ] Merge checklist.

Commit: `test(dev): add gslfs endian and list error tests`.

## Status akhir sesi

| Grup | Status | Commit |
|---|---|---|
| 0 Baseline | selesai | `4400e7a` |
| 1 Shared primitives | selesai | `54ae45b` |
| 2 Filesystem | selesai | `df4329a` |
| 3 Process | sebagian | `df4329a` |
| 4 VMM/scheduler | sebagian | `2f87675` |
| 5 Driver/platform | sebagian | `e6a9bfa` |
| 6 Userland | sebagian | `df4329a` |
| 7 Test/dokumentasi | sebagian | `df4329a` |

Verifikasi yang berjalan di sesi ini:

```
python3 scripts/compile-check.py
compile-check: 98 files OK (1 skipped)
host tests: all passed
ramfs probe: OK
procfs probe: OK
devfs probe: OK
fat32 probe: OK
gemapi fs probe: OK
kgema probe: OK
gemapi api probe: OK
gslfs probe: OK
gslfs test: OK
kernel build via podman: OK
arch-check: OK
gsl-check: OK
qemu test: OK
```

Full `test.sh` belum dijalankan sebagai satu perintah, tetapi komponennya
sudah lulus: build, host test, GSLFS test, dan QEMU smoke test di container
arm64.

## Aturan commit

- Jangan commit tanpa perintah eksplisit dari user. Kumpulkan perubahan
  dulu, lalu tunggu perintah commit.
- Satu grup satu commit.
- Jangan campur refactor dengan fitur.
- Compile-check hijau sebelum commit.
- Catat risiko dan verifikasi yang belum dijalankan.

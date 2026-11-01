# Clean Code Review - OpenGEMA

Tanggal: 2026-11-01.
Scope: commit `50dca01` dan skill `opengema-cleancode`.
Metode: baca kode dan diff, petakan ke bab Clean Code, jalankan
`check-gsl.sh`, `check-arch.sh`, dan `test.sh`.

Review ini adalah audit awal, bukan refactor menyeluruh. Temuan besar
dicatat sebagai backlog agar tidak mencampur perubahan berisiko dengan
perbaikan kecil.

## Ringkasan

| Bab | Temuan | Status |
|---|---|---|
| 1 Clean Code | Tidak ada `TODO`/`FIXME`; build dan test hijau | Baik |
| 2 Names | Nama umumnya mengungkap maksud; `mode` GSLFS belum dipakai | Dicatat |
| 3 Functions | `SetupWritableFs` dan `gslfs::Mount` panjang; `run_external` diperbaiki | Sebagian |
| 4 Comments | Tidak ada kode mati; komentar umumnya why | Baik |
| 5 Formatting | clang-format konsisten | Baik |
| 6 Objects/Data | Pemisahan data dan object jelas | Baik |
| 7 Error handling | `Status`/`Result`; cleanup `(void)` terkontrol | Baik |
| 8 Boundaries | Adapter block device, syscall, API jelas | Baik |
| 9 Tests | Host, QEMU, dan negative test ada; GSLFS unit test kurang | Sebagian |
| 10 Classes | Modul kohesif; setup filesystem terlalu besar | Dicatat |
| 11 Systems | Composition root kernel dan init jelas | Baik |
| 12 Emergence | Tidak ada abstraksi spekulatif | Baik |
| 13 Concurrency | virtio-blk lock sudah ada; lock ordering belum didokumentasikan | Sebagian |
| 14 Successive refinement | Milestone GSL bertahap | Baik |
| 15 JUnit internals | Harness test masih sederhana | Baik |
| 16 SerialDate | RTC/waktu belum direfactor | Ditunda |
| 17 Smells | Duplikasi path sudah dihapus; smell lain tercatat | Sebagian |

## Perbaikan yang diterapkan

### Duplikasi pembentukan path di gemabox

Sebelumnya `run_external` menyalin prefix, nama, separator, dan nama lagi
dengan loop manual yang berulang. Fungsi itu sekarang memakai helper kecil:

- `append_text`: menambahkan string ke buffer dengan batas kapasitas.
- `finish_path`: menutup path dengan NUL dan memeriksa overflow.

Dampak:

- Satu tempat untuk aturan penambahan path.
- `run_external` lebih pendek dan lebih mudah dibaca.
- Perilaku tidak berubah: `/commands/<name>` lalu
  `/applications/<name>/<name>`.

### Komentar field mode GSLFS

Field `mode` pada `GslfsNode` disimpan untuk penegakan permission nanti.
Komentar why ditambahkan agar field tidak terlihat seperti kode mati.

## Temuan yang ditunda

### P1 - Pecah `gslfs::Mount`

`kernel/fs/gslfs.cpp` memiliki fungsi `Mount` yang menangani superblock,
node table, name table, parent validation, dan data checksum. Ini beberapa
tanggung jawab dalam satu fungsi.

Usulan:

- `ReadSuperblock`
- `LoadNodeTable`
- `ValidateTree`
- `VerifyFileData`

Manfaat: lebih mudah dites dan dilacak saat mount gagal.

### P1 - Pecah `SetupWritableFs`

`kernel/core/init/filesystem.cpp` membuat tree, mount, menyalin config, dan
menguji tulis dalam satu fungsi.

Usulan:

- `CreateWritableTree`
- `MountWritableTree`
- `SeedEtcConfig`
- `VerifyTmpWrite`

### P1 - Test unit GSLFS

Saat ini GSLFS diuji lewat `mkgslimg --verify` dan QEMU. Belum ada test
host yang memanggil parser/driver dengan image valid dan rusak.

Usulan test:

- Magic salah.
- Checksum superblock salah.
- Node checksum salah.
- Data checksum salah.
- Parent cycle.
- Name tanpa NUL.
- File di luar range block.

### P2 - Dokumentasi lock ordering

`IrqSpinlock` dipakai di heap, PMM, scheduler, dan virtio-blk. Urutan lock
belum didokumentasikan dalam satu tempat.

Usulan:

- Catat urutan lock di header subsystem.
- Tambah stress test spawn/read paralel di QEMU.
- Pastikan tidak ada alokasi heap di bawah lock driver.

### P2 - Test error syscall list dan spawn

`list` dan `spawn` sudah diuji jalur sukses. Tambahkan:

- `list` path tidak ada.
- `list` path file, bukan directory.
- `spawn` file non-ELF.
- `spawn` library hilang.
- `spawn` path terlalu panjang.

### P3 - Refactor waktu/RTC

RTC PL031 dan konversi waktu belum ditinjau dengan studi kasus SerialDate.
Ini bukan prioritas sampai ada fitur waktu user yang nyata.

## Checklist review yang sudah dijalankan

- [x] `./scripts/check-gsl.sh` (sebelum refactor gemabox)
- [x] `./scripts/check-arch.sh` (sebelum refactor gemabox)
- [x] `./scripts/test.sh` (sebelum refactor gemabox, hijau)
- [x] Shell `help`, `ls /`, `hello`, `exit` (sebelum refactor gemabox)
- [x] GSLFS mount dan checksum
- [x] Frame balance
- [x] Tidak ada em dash atau Unicode non-keyboard

Refactor `run_external` hanya mengubah pembentukan path di user space dan
sudah dikompilasi dengan `clang++ --target=aarch64-linux-gnu`. Full
`test.sh` belum dijalankan ulang setelah refactor karena podman dan QEMU
tidak tersedia di sesi ini. Jalankan `./scripts/test.sh` di environment
build sebelum merge.

## Refactor branch dev

Perubahan yang sudah dikerjakan di branch `dev`:

- `libs/core/endian.h` menjadi satu sumber reader little-endian.
  `efi_image.cpp`, `gslfs.cpp`, `partition.cpp`, dan `elf.cpp` memakainya.
- `ldgem/text.h` dan `text.cpp` menjadi satu sumber helper teks untuk
  `ld-gem`.
- `gslfs::Mount` dipecah menjadi `ReadSuperblock`, `LoadNodes`,
  `ValidateTree`, dan `VerifyNodeData`.
- `SetupWritableFs` dipecah menjadi `CreateWritableTree`,
  `MountWritableTree`, `SeedEtcConfig`, dan `VerifyTmpWrite`.
- `SpawnWithArgs` memakai `BuildInterpreterEnvironment` agar state
  environment tidak berserakan.
- `vfs.cpp` memisahkan `FindBestMount` dan `ResolveSegments`.
- FD table dipindah ke `fd_table.cpp`; `vfs_internal.h` hanya membawa
  `IsReady()` untuk kode kernel, bukan header publik.
- `fd_table.cpp` meng-include `og/fs/vfs.h` yang mendeklarasikan fungsi
  yang diimplementasikannya, sesuai aturan include header di .cpp.
- `fdt.cpp` memakai satu `NodeBodyOffset` untuk prolog node.
- `docs/LOCKING.md` mencatat urutan lock yang diketahui: heap -> PMM,
  scheduler -> PMM, dan virtio-blk sebagai leaf.
- `VmmMapDevice` diperbaiki formatting-nya.
- Test host baru: endian reader, image GSLFS valid/rusak/deterministik,
  driver GSLFS, ramfs, procfs, GPT/partition, VFS, VFS FD, ELF parser, dan
  FDT parser.
- Test QEMU baru: `ls /missing` diharapkan menghasilkan
  `ls: list failed`.

Verifikasi branch `dev`:

- `python3 scripts/compile-check.py`: 90 file OK, 1 entri lama dilewati.
- `./scripts/test-host.sh`: host tests, ramfs, procfs, GPT, FDT, VFS, ELF,
  dan GSLFS probe OK.
- `test.sh` penuh belum dijalankan karena podman/QEMU tidak tersedia.

## Kesimpulan

Basis kode sudah mengikuti banyak prinsip Clean Code: nama jelas, modul
kecil, error eksplisit, layer terjaga, dan test end-to-end. Perbaikan
terbesar berikutnya bukan menambah fitur, tetapi memecah dua fungsi besar
dan menambah test unit untuk GSLFS.

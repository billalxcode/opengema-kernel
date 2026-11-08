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
`test.sh` belum dijalankan sebagai satu perintah, tetapi build, host test,
GSLFS test, dan QEMU smoke test sudah lulus lewat podman container.

## Refactor branch dev

Perubahan yang sudah dikerjakan di branch `dev`:

- `libs/core/endian.h` menjadi satu sumber reader little-endian.
  `efi_image.cpp`, `gslfs.cpp`, `partition.cpp`, dan `elf.cpp` memakainya.
- `ldgem/text.h` dan `text.cpp` menjadi satu sumber helper teks untuk
  `ld-gem`.
- Relocation `ld-gem` dipisah ke `packages/ld-gem/src/linker_reloc.cpp`;
  dynamic parse dipisah ke `linker_dynamic.cpp`, dan `linker.cpp` menangani
  object table serta symbol lookup.
- Applet gemabox dipisah menjadi `shell.cpp` dan `writer.cpp`; `main.cpp`
  hanya menangani mode proses dan self test EL0. Shell memakai tabel
  `kCommands` sebagai registry, dan daftar `help` dibangun dari tabel itu.
- Command eksternal menerima path langsung dan argumen; pencarian otomatis
  tetap mencoba `/commands/<nama>` lalu `/applications/<nama>/<nama>`.
- `gemapi::fs` memakai `ConvertKind` untuk `stat` dan `list`.
- `gslfs::Mount` dipecah menjadi `ReadSuperblock`, `LoadNodes`,
  `ValidateTree`, dan `VerifyNodeData`.
- `SetupWritableFs` dipecah menjadi `CreateWritableTree`,
  `MountWritableTree`, `SeedEtcConfig`, dan `VerifyTmpWrite`.
- `InitFilesystem` hanya mengatur urutan; detailnya ada di
  `FindBlockDevice`, `InitFileTables`, `MountSystemVolume`, `ProbeLibrary`,
  `MountDevfs`, `MountOptionalFilesystems`, `ProbeConsole`, dan `ListRoot`.
- `SpawnWithArgs` memakai `BuildInterpreterEnvironment` agar state
  environment tidak berserakan.
- `kernel/process/loader.cpp` memisahkan baca image, map segment, dan
  interpreter path; `SpawnWithArgs` memakai `LoadProgram` dan
  `CreateProcess`.
- `kernel/syscall/io.cpp` memisahkan `DoRead`, `DoWrite`, dan `TtyReadLine`;
  deklarasi bersama ada di `og/syscall/syscall_internal.h`.
- `kernel/syscall/fs.cpp` memisahkan handler filesystem, dan
  `kernel/syscall/helpers.cpp` menyimpan `CurrentProcess`, `CopyUserString`,
  serta `CopyUserPath`.
- `kernel/syscall/process.cpp` memisahkan `DoGetPid`, `DoWait`, dan
  `DoSpawn`.
- `kernel/syscall/memory.cpp` memisahkan `DoMmap` dan `DoMunmap`;
  `syscall.cpp` tinggal berisi dispatch dan inisialisasi.
- `vfs.cpp` memisahkan `FindBestMount` dan `ResolveSegments`.
- `partition.cpp` memisahkan `ReadGptHeader`, `ParsePartition`, dan
  `CheckMagic` dari loop pencarian.
- `gslfs.cpp` memisahkan `LoadNode` sehingga loop `LoadNodes` hanya
  mengatur state global.
- `gslfs::LoadNode` memakai `ReadNodeRecord`, `ParseNodeFields`,
  `AssignNodeName`, dan `AssignNodeExtent` untuk satu node.
- `gslfs::ValidateTree` memakai `ReachesRoot`, dan `DirIterate` memakai
  `FillDirEntry`.
- `gslfs::ReadSuperblock` memakai `ParseSuperblockFields`,
  `CheckSuperblockHeader`, dan `CheckSuperblockLayout`.
- `fat32::Mount` memisahkan `ParseBootSector` dari penyiapan volume dan
  root node.
- `fat32::WalkDirectory` memakai `ParseEntry` untuk satu slot direktori,
  termasuk nama pendek, long name, dan volume label.
- `fat32::FileRead` memakai `SeekCluster` dan `AdvanceCluster` sehingga
  penelusuran rantai cluster tidak diduplikasi.
- `devfs.cpp` memakai satu tabel `g_devices` untuk lookup dan iteration.
- FD table dipindah ke `fd_table.cpp`; `og/fs/vfs_internal.h` hanya
  membawa `IsReady()` untuk kode kernel, bukan header publik.
- `fd_table.cpp` meng-include `og/fs/vfs.h` yang mendeklarasikan fungsi
  yang diimplementasikannya, sesuai aturan include header di .cpp.
- Semua header baru disimpan di `kernel/include/og/...`; tidak ada header
  yang diletakkan di samping file `.cpp`.
- `fdt.cpp` memisahkan state/tokenizer (`fdt_state.cpp`) dan property
  access (`fdt_property.cpp`); helper internal ada di
  `og/platform/fdt_internal.h`.
- `fdt.cpp` memakai satu `NodeBodyOffset` untuk prolog node.
- `docs/LOCKING.md` mencatat urutan lock yang diketahui: heap -> PMM,
  scheduler -> PMM, dan virtio-blk sebagai leaf.
- `VmmMapDevice` diperbaiki formatting-nya.
- `Queue` virtio dipisah ke `kernel/drivers/virtio/virtio_queue.cpp`;
  layout descriptor ada di `og/drivers/virtio_internal.h`.
- HID decoder generik ditambahkan untuk keyboard dan mouse; virtio-input
  menginisialisasi event queue dan menerjemahkan event evdev ke event HID.
- Input keyboard masuk ke queue TTY `og/io/tty.h`; `TtyReadLine` membaca
  queue keyboard dan UART. QEMU test mengetik `hello` dari keyboard.
- `FramebufferConsole` dipisah menjadi `framebuffer_console.cpp` (console),
  `framebuffer_draw.cpp` (drawing), dan `font.cpp` (font 8x8).
- `VmmCopyIn`, `VmmCopyOut`, dan `VmmUserContains` dipindah ke
  `kernel/mem/vmm_user.cpp`; mapping dan page table tetap di `vmm.cpp`.
- `VmmQueryAttributes`, `VmmTranslate`, dan `VmmTranslateIn` dipindah ke
  `kernel/mem/vmm_query.cpp`; helper internal ada di `og/mem/vmm_internal.h`.
- `AddressSpaceInit` dan `AddressSpaceDestroy` dipindah ke
  `kernel/mem/vmm_space.cpp`; aktivasi per-CPU tetap di `vmm.cpp`.
- Wait queue, `WakeThread`, dan `ScheduleCurrent` dipindah ke
  `kernel/sched/wait_queue.cpp`; akses internal lewat
  `og/sched/scheduler_internal.h`.
- Run queue (`EnqueueLocked`, `DequeueLocked`, `RemoveLocked`, dan
  `PickHomeLocked`) dipindah ke `kernel/sched/run_queue.cpp`.
- Thread creation dipindah ke `kernel/sched/thread.cpp`; alokasi slot dan
  ID tetap di `scheduler.cpp` lewat helper internal.
- Test host baru: endian reader, image GSLFS valid/rusak/siklus/deterministik,
  driver GSLFS, ramfs, procfs, devfs, HID, ACPI, FAT32, gemapi fs, gemapi api,
  kgema, GPT/partition, VFS, VFS FD, ELF parser, dan FDT parser.
- Test QEMU baru: `ls /missing` menghasilkan `ls: list failed`, dan
  perintah `.` yang menunjuk directory menghasilkan `run: spawn failed`.
- Test QEMU power: `shutdown` mematikan QEMU dan `reboot` memicu boot kedua
  lewat ACPI FADT dan PSCI.
- Test QEMU waktu: `sleep` menunggu dan `uptime` mencetak waktu sejak boot.
- Test QEMU proses: `ps` mencetak header dan daftar proses dari kernel.
- Test QEMU init array: konstruktor global `hello` berjalan sekali per
  eksekusi dynamic.
- Test SDK: install package dan contoh `examples/hello` berhasil dibangun
  sebagai ELF64 AArch64 memakai `find_package(gemapi)`.
- Test FD flags: tulis ditolak pada descriptor read-only dan append menulis
  di akhir berkas.
- Test VFS mutation: mkdir, unlink, rmdir, rename, duplicate create, mount
  root yang tidak boleh dihapus, dan tidak ada kebocoran alokasi node.
- Test mount table: mount `ramfs` berrefcount, unmount melepas entry terakhir,
  root kernel yang pinned tidak bisa di-unmount, dan direktori di bawah mount
  kembali terlihat setelah unmount. Descriptor yang masih terbuka menahan
  root sampai ditutup, lalu node dibebaskan.
- Test dmesg: `/proc/dmesg` dibaca lewat procfs dan command shell.
- Test CPU accounting: `/proc/processes` menampilkan kolom `cpu_ms`, uid,
  gid, jumlah region user, dan prioritas thread lewat kolom `prio`;
  `/proc/stat` menampilkan heartbeat per CPU; `/proc/threads` menampilkan
  snapshot id, CPU, state, prioritas, ticks, TLS, dan nama thread.
- Test prioritas user: `hello prio` membaca prioritas default 16,
  menurunkannya ke 20, menolak nilai di atas 31, dan memastikan proses
  non-root ditolak saat menaikkan prioritas. Shell `prio` menampilkan
  prioritas thread yang sama.
- Test work stealing: empat worker migratable dibuat di CPU 0; CPU idle
  lain mencuri minimal satu worker, dan self-test memastikan bit CPU selain
  0 muncul di mask eksekusi. Worker `ThreadCreateOn` tanpa migratable tetap
  pinned.
- Test sync: condition variable wait/signal dan read/write lock dijalankan
  di self-test scheduler.
- Test observability: `/proc/meminfo` menampilkan frame dan heap,
  `/proc/stat` menampilkan tick, thread, context switch, proses, operasi
  VFS, dan counter block device. `/proc/trace` menampilkan ring event,
  `/proc/devices` menampilkan registry device, `/proc/interrupts`
  menampilkan jumlah interrupt per IRQ, dan `/proc/mounts` menampilkan
  mount table beserta referensinya.
- Test SysRq: `Ctrl-A` lalu `s` lewat UART mencetak statistik kernel;
  `b`, `o`, dan `h` menjadwalkan reboot, shutdown, dan halt.
- Test user fault: program `hello fault` menyentuh alamat 0, kernel mencatat
  fault, mematikan proses, dan shell tetap berjalan.
- Test pointer invalid: `hello badptr` memanggil `open` dengan pointer `1`,
  kernel menolaknya, dan proses tetap hidup.
- Test fuzz argumen: `hello fuzz` menjalankan 512 panggilan dengan pointer,
  ukuran, flag, argc, dan pid acak deterministik. Semua kegagalan ditolak
  tanpa panic dan program mencetak `FUZZ-DONE`.
- Test memory limit: `hello mem` memetakan 8 MiB, melepasnya, lalu map 40 MiB
  ditolak oleh limit 32 MiB. `/proc/processes` menampilkan `mem_kb`,
  `limit_kb`, uid, dan gid per proses.
- Test ASLR user: `hello aslr` dijalankan dua kali; alamat stack, mmap, dan
  code harus berbeda. Stack diacak 8 MiB, mmap 16 MiB, dan executable
  memakai PIE/ET_DYN dengan load bias 128 MiB.
- Test stack growth: `hello stack` memakai frame 4 KiB dan tumbuh melewati
  stack awal 16 KiB sampai batas 64 KiB. `hello stackguard` melewati batas
  tersebut dan harus mati pada guard page.
- Test TLB shootdown: setiap perubahan permission atau unmap user mengirim
  SGI `kIpiIrq` ke CPU online lain. Handler IPI mem-flush TLB lokal lalu
  menyimpan ack generasi. Pengirim menunggu ack dengan timeout 1 ms agar
  target yang sibuk atau IRQ-masked tidak membuat deadlock.
- Test ASID: self-test mengalokasikan 255 address space, memastikan space
  ke-256 ditolak, menghancurkan semuanya, lalu mengalokasikan ulang. Bitmap
  ASID mengembalikan slot saat `AddressSpaceDestroy`, jadi tidak ada
  ekshausti permanen setelah 255 proses.
- Test PAN: `-cpu max` menyalakan PAN (`hardening: pan enabled`) dan
  seluruh tes lulus; cortex-a72 melaporkan `pan unsupported`. Self-test
  address space memakai `VmmCopyIn`, bukan membaca VA user langsung.
- Test boot matrix: QEMU SMP=1/2/4 dengan memori 256M/512M, ACPI on/off,
  dan CPU `max` untuk PAN. Jalur SMP=1 melewati IPI cross-core.
- Test errno: `hello errno` memastikan path hilang menghasilkan `not found`,
  pointer invalid menghasilkan `invalid argument`, dan map 40 MiB
  menghasilkan `out of memory`. Host test memeriksa `from_errno`.
- Test thread join: self-test membuat worker, menunggu lewat `ThreadJoin`,
  lalu memastikan stack dan slot thread sudah diklaim kembali.
- Test fork: `hello fork` membuka berkas, memanggil `fork`, lalu child
  menulis global dan membaca FD; parent memastikan global lama dan offset
  FD sendiri. Page user dibagi COW dengan refcount frame.
- Test exec: `hello exec` mengganti image proses dengan hello baru dan argv
  `exec-child`. Image lama tidak kembali; shell melanjutkan setelah proses
  baru keluar.
- Test lazy mmap: `hello lazy` memetakan 8 MiB tanpa frame, menyentuh dua
  halaman, lalu `/proc/meminfo` menunjukkan `lazy_faults: 2`.
- Test shared mmap: `hello shm` berbagi satu halaman anonim dengan child
  melewati `fork`; tulisan child terlihat oleh parent.
- Test partial munmap: `hello unmap` melepas halaman tengah dari mapping
  tiga halaman, memastikan tetangga tetap valid, lalu child yang menyentuh
  halaman tengah harus mati.
- Test futex: `hello futex` memakai shared page, child memblokir di
  `futex_wait` sampai parent menulis nilai baru dan memanggil `futex_wake`.
  Parent harus membangunkan tepat satu waiter.
- Test TLS: `hello tls` mengatur TPIDR_EL0, memaksa context switch, lalu
  memastikan register dan isi blok tetap sama. Child hasil `fork` harus
  mewarisi nilai TLS parent.
- Test file-backed mmap: `hello filemap` memetakan snapshot private
  `/system/config/version`, memeriksa isi dan zero tail, lalu memastikan
  tulisan ke mapping tidak bocor ke berkas.
- OOM killer minimal membunuh proses yang gagal `mmap` saat free frame di
  bawah reserve 4 MiB. Jalur ini belum dipicu oleh tes QEMU.
- CI GitHub Actions menjalankan host test, sanitizer, clang-tidy, coverage,
  build native, SDK probe, image boot, smoke test QEMU DTB dan ACPI, serta
  persistensi.
- Host test bisa dijalankan dengan ASan+UBSan lewat
  `scripts/test-sanitize.sh`. Seluruh probe lulus; `ramfs_probe` sekarang
  melepas root tree lewat `NodeOps::release` sehingga tidak ada leak.
- Coverage source-based LLVM untuk host test inti menghasilkan 56,96 persen
  baris dengan ambang CI 55 persen. Laporan ada di
  `build/coverage/report.txt`.
- Firmware UEFI dipin di `scripts/firmware.lock` ke paket Debian
  `qemu-efi-aarch64` 2022.11-6+deb12u2. `get-firmware.sh` memverifikasi
  SHA256 paket dan kedua file AAVMF, lalu mengunduh ulang bila berbeda.
  Ini juga membersihkan variable store yang sempat berubah karena boot lama.
- Workflow mengunggah artifact `boot-image` berisi `esp.img`,
  `BOOTAA64.EFI`, dan `gema-system.img`.
- `fix_compile_commands.py` sekarang hanya mengganti `/work` sebagai
  komponen path. Sebelumnya penggantian substring merusak entri
  `kernel/events/workqueue.cpp` menjadi path ganda.
- Test W^X user: `hello wx` mencoba menulis ke halaman code; proses fault,
  shell tetap berjalan, dan tidak ada `code write allowed`.
- Test NX: `hello nx` memanggil instruksi dari halaman data RW NX; proses
  fault, shell tetap berjalan, dan tidak ada `nx: data executed`.
- Test mprotect dynamic linker: ld-gem memetakan shared object writable,
  menyalin segmen, lalu memanggil `mprotect` agar teks RX dan data RW NX;
  seluruh program dynamic tetap berjalan.
- Test watchdog: timer periodik membandingkan heartbeat tiap CPU dan panic
  bila salah satu CPU berhenti menerima tick.
- Test SMP: `SMP=4` membuat worker thread satu per CPU dan memverifikasi
  seluruh CPU menjalankan worker. Ping-pong semaphore 200 kali menguji lost
  wakeup, dan sleep overshoot dicatat sebagai metrik latency.
- Test loglevel: filter runtime bisa dibaca dan diubah dari shell.
- Test wall clock: `date <unix-seconds>` menyetel RTC PL031, `date`
  mencetak waktu lokal, dan `tz <menit>` menggeser offset dengan format
  `YYYY-MM-DD HH:MM:SS UTC+HH:MM`. `tz save` menulis offset ke
  `/volumes/boot/gsl.tz` dan nilainya dimuat lagi setelah reboot.
- Verifikasi manual block write: baca, tulis pola, baca ulang, lalu pulihkan
  sector terakhir pada salinan `esp.img`.
- Test persistensi: aplikasi `/applications/fstest/fstest` menjalankan
  `write`/`create`/`mkdir`/`delete`/`rename`/`truncate`/`long`, lalu reboot
  dan `read`/`read2`/`empty`/`readlong` dari salinan image yang sama tanpa
  cache `writethrough`. Rename dan delete nama panjang memakai `mv`/`rm`
  berkutip. Command `persist` tidak lagi ada di shell produksi.
- Test shell quoting: `mkdir` dan `rm` menerima path dengan spasi di dalam
  tanda kutip ganda.
- Test metadata: `stat` menampilkan tipe, ukuran, mode oktal, uid, gid, dan
  mtime; mode direktori, file, device, dan file read-only diperiksa.
  `chmod` memperbarui mode ramfs dan read-only attribute FAT32, `chown`
  memperbarui owner ramfs, dan proses non-root ditolak saat membuka file
  0644 untuk write atau mengubah file milik user lain.
- Test mount di QEMU: `mount ramfs` dua kali, dua `umount`, lalu `rm`
  direktori target untuk memastikan refcount dan cleanup berjalan.
- Test multi block device: QEMU memakai dua virtio-blk, kernel mencatat
  kapasitas keduanya, dan `/proc/block` menampilkan `virtio-blk0` serta
  `virtio-blk1`. Capability MSI-X dibaca dan jumlah vector dicatat.
- Test device resource: `/proc/devices` menampilkan urutan probe, MMIO,
  IRQ, clock, DMA, dan kapasitas tiap device.

Verifikasi branch `dev`:

- `python3 scripts/compile-check.py`: 141 file OK, 0 dilewati.
- `./scripts/build.sh` lewat podman: kernel build dan link OK,
  `arch-check` dan `gsl-check` OK.
- `./scripts/test-tidy.sh`: 121 file C++ OK dengan clang-tidy.
- Sanitizer host test dengan ASan+UBSan: seluruh probe OK.
- `./scripts/test-coverage.sh`: host test inti 55,41 persen baris,
  ambang 55 persen.
- `hello mem`: map 8 MiB lalu unmap berhasil, map 40 MiB ditolak limit
  32 MiB, dan `/proc/processes` menampilkan `mem_kb` serta `limit_kb`.
- `hello aslr` dua kali: alamat stack, mmap, dan code berbeda. Aplikasi
  dynamic adalah ET_DYN dan kernel memilih load bias acak.
- `hello stack`: growth melewati 16 KiB awal berhasil. `hello stackguard`
  melewati 64 KiB dan mati pada guard page tanpa panic kernel.
- NX user: `hello nx` memanggil halaman data RW NX; kernel mencatat user
  fault, proses mati, dan `NX-DONE` muncul tanpa `nx: data executed`.
- TLB shootdown: `smp: ipi count: 1`, `smp: tlb sync`, dan
  `smp: tlb shootdown ok` muncul; QEMU SMP=4 lulus tanpa deadlock.
  `/proc/meminfo` menampilkan `tlb_sync` dan `tlb_timeouts`.
- Reuse ASID: `mem: asid reuse ok` muncul setelah self-test mengisi dan
  mengosongkan seluruh 255 slot ASID.
- PAN: QEMU `-cpu max` mencetak `hardening: pan enabled` dan lulus
  ACPI/SMP=4; cortex-a72 mencetak `hardening: pan unsupported`.
- Boot matrix: SMP=1/2/4 dengan 256M/512M, DTB/ACPI, dan CPU=max lulus.
  SMP=1 mencetak `smp: single core` tanpa IPI.
- Errno: `hello errno` mencetak `not found`, `invalid argument`, dan
  `out of memory`; host probe memetakan errno negatif ke `gemapi::Status`.
- Thread join: `sched: join ok` muncul dan stack worker kembali ke PMM.
- Prioritas scheduler: `sched: priority ok` muncul setelah worker prioritas
  0 berjalan sebelum worker prioritas 10 meskipun dibuat belakangan.
  `/proc/processes` menampilkan kolom `prio`.
- Prioritas user: `prio: ok` dan `PRIO-DONE` muncul; `hello prio` menurunkan
  prioritas ke 20, menolak 40, dan menolak kenaikan oleh uid 1. Shell
  mencetak `priority: 16` sebelum perubahan.
- Work stealing: `sched: steal ok` muncul pada SMP=2/4 setelah worker dari
  CPU 0 berjalan juga di CPU lain; SMP=1 melewati tes ini.
- Fork: `fork: child` dan `fork: parent` muncul; child menulis global COW
  tanpa mengubah nilai parent, dan kedua proses membaca FD hasil clone
  dari offset masing-masing.
- Exec: `exec: child` muncul dari image baru dan `EXEC-DONE` menyusul;
  PID, FD, cwd, uid, dan gid dipertahankan.
- Lazy mmap: `lazy: ok` dan `lazy_faults: 2` muncul setelah dua halaman
  disentuh; frame baru dialokasikan saat fault.
- Shared mmap: `shm: child` lalu `shm: parent` membuktikan halaman anonim
  yang sama tetap writable dan dibagi setelah `fork`.
- Partial munmap: `unmap: ok` muncul setelah halaman tengah dilepas,
  tetangga tetap berisi pola, dan child yang menyentuh halaman tengah mati.
- Futex: `futex: child woke` lalu `futex: parent` muncul; `futex_wake`
  mengembalikan satu waiter dan child melanjutkan setelah nilai berubah.
- TLS: `tls: child` lalu `tls: parent` muncul; TPIDR_EL0 dan blok TLS tetap
  valid setelah sleep, context switch, dan `fork`.
- File-backed mmap: `filemap: ok` muncul setelah snapshot private berisi
  `0.1`, tail nol, dan tulisan ke mapping tidak mengubah berkas asli.
- `.github/workflows/ci.yml`: build native Linux, host test, sanitizer,
  clang-tidy, coverage, SDK, QEMU DTB/ACPI, dan persistensi. Hijau pada run
  `42389553053` di branch `dev`.
- `./scripts/get-firmware.sh`: firmware 2022.11-6+deb12u2 terverifikasi
  SHA256; artifact `boot-image` berisi loader dan image sistem.
- `./scripts/test-host.sh`: host tests, ramfs, procfs, devfs, HID, ACPI, FAT32,
  gemapi fs, gemapi api, kgema, GPT, FDT, VFS, ELF, dan GSLFS probe OK.
- `./scripts/test-qemu.sh` lewat container arm64: OK.
- `test.sh` penuh belum dijalankan sebagai satu perintah.

## Kesimpulan

Basis kode sudah mengikuti banyak prinsip Clean Code: nama jelas, modul
kecil, error eksplisit, layer terjaga, dan test end-to-end. Perbaikan
terbesar berikutnya bukan menambah fitur, tetapi memecah dua fungsi besar
dan menambah test unit untuk GSLFS.

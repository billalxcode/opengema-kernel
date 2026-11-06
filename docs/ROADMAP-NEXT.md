# Roadmap lanjutan opengema-kernel

Status: Fase 0 sampai 14 selesai. Dokumen ini merencanakan fase 15 dan
seterusnya, setelah kernel bisa boot di QEMU virt, menjalankan dua core,
memakai address space per proses, dan menjalankan gemabox di EL0.

## 1. Analisis kesenjangan

Prioritas: P0 blocker, P1 penting, P2 menengah, P3 nanti.

| Area | Kondisi sekarang | Yang kurang | Dampak | Prioritas |
|---|---|---|---|---|
| Model proses | Banyak proses, PID, parent, wait, fork COW, exec, exit status, argv, cwd, reklamasi | env, daftar anak, banyak thread per proses | Model proses inti lengkap | P0 |
| File descriptor | FD per proses, open, read, write, dan stdout | Open flags lengkap, append, create, truncate | Program belum bisa membuat berkas baru | P0 |
| Syscall | open, close, read, write stdout, stat, seek, getpid, wait, spawn, exit, mmap | Write file, exec, error/errno konsisten, validasi lengkap | Userland belum bisa menjalankan program lain dengan argumen | P0 |
| Reklamasi | Stack, address space, FD, dan slot proses dibebaskan; frame kembali | PID reuse, reklamasi saat gagal di tengah jalan | Aman untuk dua proses, belum untuk beban besar | P0 |
| SDK dan shell | API berkas/proses/waktu/error, argv, konstruktor C++, shell multi perintah | Packaging SDK, contoh program, dan template package | Belum ada pengalaman userland yang nyata | P1 |
| Filesystem | FAT32 read-only, ramfs statis, mount internal | Tulis berkas, mkdir, unlink, rename, mount/umount, permission, sync | Data tidak bisa disimpan atau diubah | P1 |
| Memori user | W^X user, stack growth 64 KiB, guard page, limit 32 MiB, ASLR stack/mmap/executable, fork COW, lazy mmap, OOM kill requester | File-backed mmap, ELF demand paging, OOM selektif | Demand paging baru untuk mmap anonymous | P1 |
| Scheduler | Per-CPU, thread tidak migrasi, join dan reclaim, tanpa prioritas | Migrasi/work stealing, prioritas, futex, TLS | Beban tidak seimbang, primitif sinkronisasi kurang | P1 |
| TLB dan SMP | ASID, flush lokal saat switch, IPI shootdown non-blocking | Ack sinkron, shootdown selektif per ASID, reuse ASID aman | Flush remote belum sinkron, tapi tidak deadlock | P1 |
| Driver | UART, RTC, PCI, virtio-blk, virtio-gpu | virtio-net, input, MSI-X, virtio IRQ, multi device, DMA streaming | Belum ada jaringan atau input interaktif | P1 |
| Platform | QEMU virt hardcoded, DTB/ACPI dasar, matriks 1/2/4 core 256M/512M | GTDT detail, ACPI memory map, discovery RAM/device, initrd, cmdline | Sulit pindah board atau boot ACPI penuh | P2 |
| Daya dan waktu | RTC dibaca saat boot, shutdown/reboot ACPI dan PSCI | settime, timezone, shutdown dari service manager | Mesin bisa dimatikan dan di-reboot dari shell | P2 |
| Observability | Log serial, panic, procfs uptime/threads | dmesg ring, tracing, counter, watchdog, statistik memori/CPU | Debug produksi dan regresi sulit | P1 |
| Test dan CI | Smoke QEMU, host test, sanitizer, coverage 58%, clang-tidy, fuzz, workflow CI, artifact image, firmware terkunci | Fuzz coverage-guided, coverage seluruh probe, artifact SDK | CI dasar lengkap, workflow belum run di GitHub | P1 |
| Keamanan | W^X kernel/user, stack canary, PAN bila didukung, fuzz argumen | KASLR, PAC/BTI, fuzz coverage-guided | PAN default target belum ada, validasi user terpusat | P1 |
| Dev experience | compile_commands sudah mencakup packages | SDK install/export, contoh program, template package | Program baru masih menyalin konfigurasi | P2 |
| Jaringan | Tidak ada | Ethernet, ARP, IPv4, ICMP, UDP, socket | Tidak ada konektivitas | P2 |

## 2. Prinsip roadmap lanjutan

1. Setiap fase harus menjaga build, host test, dan smoke test QEMU tetap hijau.
2. Tidak ada fitur baru di atas global state baru. Proses dan FD harus
   per objek, bukan singleton baru.
3. Setiap syscall punya validasi pointer, region, dan hak akses.
4. Setiap fase menambah minimal satu test yang gagal sebelum perbaikan.
5. Tidak menambah driver sebelum device model, resource, IRQ, dan DMA
   punya kontrak yang jelas.
6. Kode userland tetap freestanding dan tidak menambah ketergantungan libc.
7. Setiap phase ditutup dengan update dokumen ini dan commit terpisah.

## 3. Milestone

| Milestone | Fase | Hasil |
|---|---|---|
| v0.2 proses | 15, 16 | Banyak proses, wait, argv, shell dasar |
| v0.3 storage | 17 | Berkas bisa dibuat dan disimpan |
| v0.4 device | 18 | Jaringan UDP dan input keyboard |
| v0.5 scale | 19, 20 | Memori demand paging dan scheduler seimbang |
| v0.6 platform | 21 | ACPI, initrd, shutdown, board kedua |
| v1.0 quality | 22 | CI, fuzzing, observability, hardening |

## 4. Fase 15 - Model proses dan syscall inti

Tujuan: satu program user menjadi satu proses nyata, bisa dibuat, ditunggu,
dan dibersihkan.

Status saat ini: inti fase sudah jalan. Dua proses gemabox berjalan di
address space berbeda, punya tabel FD sendiri, menerima argv, menjalankan
child, menunggu, dan seluruh frame kembali setelah proses selesai. Proses
ketiga menguji cwd dan write ke FD.

- [x] PRC-01 `Process`: PID, address space, state, exit status, parent,
      FD table, nama program, dan cwd. Daftar anak belum.
- [x] PRC-02 Pisahkan kernel thread dari proses. Thread menyimpan pointer
      proses lewat field `process`.
- [x] PRC-03 Pindahkan tabel FD dari VFS global ke proses. VFS hanya
      menyimpan node; FD menyimpan offset dan flags.
- [x] PRC-04 Syscall `open`, `close`, `read`, `write`, `stat`, `seek`,
      `getpid`, `chdir`, `exit(status)`, dan `waitpid` sudah ada.
      Descriptor 0, 1, dan 2 dicadangkan untuk stdio.
- [~] PRC-05 Loader flat dari filesystem dan argv sudah. ELF64 dan env
      yang terisi belum.
- [x] PRC-06 Lifecycle: zombie, wait, reklamasi stack, address space, FD,
      dan slot proses. PID tidak dipakai ulang.
- [x] PRC-07 `spawn` dengan argv, `wait`, `fork`, dan `exec` sudah
      dipakai userland.
- [x] PRC-08 Uji: dua proses berjalan bersamaan, masing masing spawn child
      dengan argv, parent wait, writer menguji cwd dan write FD, exit status
      benar, dan jumlah frame bebas kembali ke angka sebelum spawn.

**Selesai bila:** kernel bisa menjalankan dua program berbeda, parent
menunggu anak, dan tidak ada kebocoran thread, stack, page, atau FD.
Target itu sudah tercapai untuk dua proses gemabox yang sama.

Catatan implementasi:

1. `SP_EL0` sekarang disimpan per thread. Tanpa itu, preemption antara dua
   proses EL0 membuat proses kedua memakai stack proses pertama.
2. Heap kernel sekarang memakai spinlock. Dua proses yang spawn bersamaan
   sebelumnya merusak free list heap.
3. argv disusun di stack user sebagai string, array pointer, dan null
   terminator. `_start` menerima argc, argv, dan envp lewat x0 sampai x2.
4. Descriptor 0, 1, dan 2 dicadangkan untuk stdio. File pertama dimulai
   dari fd 3, sehingga stdout tidak bertabrakan dengan berkas.
5. Path relatif digabung dengan cwd per proses. `chdir` memvalidasi bahwa
   target adalah directory sebelum menyimpan path.
6. PMM juga memakai spinlock. Alokasi page dari beberapa core sekaligus
   tanpa lock membuat hitungan frame dan bitmap tidak konsisten.

Langkah berikutnya untuk fase ini: ELF64, env, daftar anak, `exec`, lalu
shell gemabox.

## 5. Fase 16 - SDK v2 dan shell gemabox

Tujuan: userland punya API yang layak dan gemabox menjadi shell nyata.

- [x] SDK-01 `libkgema` API berkas: open, read, write, close, stat, seek,
      dan chdir sudah.
- [x] SDK-02 API proses: getpid, wait, exit, spawn, dan argv sudah.
- [x] SDK-03a API waktu: `now_ms` dan `sleep_ms` di libkgema dan libgemapi.
- [x] SDK-03b Error/errno yang konsisten untuk semua syscall. Kernel
      mengembalikan errno negatif dan libgemapi memetakannya ke `Status`.
- [x] SDK-03 API waktu dan error yang konsisten, tanpa libc.
- [x] SDK-04 Jalankan `.init_array` di `_start`, dukung konstruktor global
      sederhana tanpa exception dan tanpa RTTI.
- [x] SDK-05 `_start` meneruskan argc/argv ke `main`.
- [x] SDK-06 gemabox shell: `help`, `echo`, `cat`, `ls`, `clear`, `acpi`,
      `uptime`, `sleep`, `ps`, `halt`, `shutdown`, dan `reboot` sudah.
- [x] SDK-07 Input UART ke TTY: line buffer, backspace, echo, read blocking.
- [x] SDK-08 Packaging: install header dan `libkgema.a`, CMake package
      config, satu contoh program baru, dan dokumentasi link.
- [x] SDK-09 Uji: shell menerima `help`, `ls`, `acpi`, `uptime`, `sleep`,
      `ps`, `shutdown`, `reboot`, dan `halt`.

**Selesai bila:** shell interaktif berjalan di atas UART dan perintah dasar
bisa dijalankan tanpa menyalin kode syscall.

## 6. Fase 17 - Storage dan VFS

- [x] FS-10 FD dan open flags per proses, termasuk read/write/append.
- [x] FS-11 Operasi directory: mkdir, rmdir, unlink, rename, lookup.
- [x] FS-12 ramfs dinamis: node dan data dialokasikan serta dibebaskan.
- [x] FS-13 Filesystem writable: FAT32 write, create file, mkdir, delete,
      rename, truncate, long name create/read, serta cleanup entry LFN saat
      rename/delete. Catatan: rename ke nama panjang baru belum didukung.
- [x] FS-14 mount/umount syscall dan mount table global berrefcount.
      Mount hanya mendukung `ramfs`, tiap proses mencatat referensinya, dan
      entry dilepas saat proses keluar. Unmount menyembunyikan nama mount
      lebih dulu, lalu root ramfs dibebaskan setelah descriptor terakhir
      ditutup dan tidak ada operasi VFS yang sedang memakai node tersebut.
- [~] FS-15 Block layer: write path, adapter, flush, dan beberapa
      virtio-blk selesai. `/proc/block` menampilkan nama dan kapasitas tiap
      device, dan capability MSI-X sudah dibaca. IRQ/MSI-X belum diaktifkan
      dan partisi tulis belum diuji terpisah.
- [x] FS-16 Metadata: mode, uid/gid, mtime, dan ukuran dilaporkan lewat
      `stat`. Open read/write memeriksa permission, `chmod`/`chown` tersedia
      untuk ramfs, chmod FAT32 memetakan read-only attribute, dan proses
      punya identity uid/gid. Catatan: FAT32 tidak menyimpan ownership dan
      filesystem read-only memakai timestamp nol.
- [x] FS-17 Uji persistensi: buat berkas, tulis, reboot, baca kembali.
- [x] TEST-01 Command diagnostik `persist` sudah dipindah ke aplikasi
      `/applications/fstest/fstest`. Shell tidak lagi punya command test,
      dan hook `renamelong`/`deletelong` sudah dihapus.

**Selesai bila:** berkas bisa dibuat dan diubah, lalu tetap ada setelah
reboot.

## 7. Fase 18 - Driver framework, jaringan, dan input

- [x] DRV-10 Registry device menampilkan nama, kelas, MMIO, IRQ, clock,
      DMA per device, kapasitas, dan urutan probe lewat `/proc/devices`.
- [ ] DRV-11 Virtio IRQ/MSI-X, completion tanpa polling.
- [ ] DRV-12 virtio-net: transmit/receive, queue, interrupt.
- [ ] NET-01 Ethernet, ARP, IPv4, ICMP echo.
- [ ] NET-02 UDP dan socket API minimal.
- [ ] NET-03 QEMU user networking dan uji echo.
- [x] DRV-13a Generic HID boot keyboard/mouse decoder dan host test.
- [x] DRV-13b Virtio-input transport, event queue, dan translasi evdev.
- [x] DRV-13c Verifikasi injeksi key QEMU lewat monitor headless.
- [x] DRV-13 virtio input keyboard dan event queue.
- [ ] DRV-13d Mouse sebagai device input kedua dan hotplug.
- [x] DRV-14 TTY input dari keyboard, bukan hanya UART.
- [x] LOG-01 Log kernel hanya ke serial; display untuk output user dan TTY.
- [x] TIME-06 RTC settime, waktu dinding, dan offset zona waktu selesai
      lewat `wall_time`/`set_wall_time`/`local_time`, command `date`/`tz`,
      dan `tz save` ke `/volumes/boot/gsl.tz` yang dimuat saat boot.
- [x] OPS-01 shutdown/reboot lewat ACPI FADT dan PSCI SYSTEM_OFF/SYSTEM_RESET.

**Selesai bila:** kernel menjawab ping/UDP dari host dan menerima input
keyboard di jendela QEMU.

## 8. Fase 19 - Memori lanjutan dan isolasi

- [~] MEM-10 Demand paging: `mmap` anonymous lazy memetakan halaman saat
      first touch. Stack growth dan copy helper ikut fault-in. ELF dan
      file-backed demand paging belum.
- [x] MEM-11 COW untuk fork. Frame punya refcount, halaman user dibagi
      read-only dengan bit COW, dan write fault menyalin ke frame privat.
- [x] MEM-12 Stack growth dan guard page user. Stack tumbuh per halaman
      sampai 64 KiB; fault di luar batas mematikan proses.
- [ ] MEM-13 File-backed mmap dan shared memory.
- [x] MEM-14 W^X user: segmen executable dipetakan RX, data/stack dan
      mmap non-executable RW NX, mmap executable RX, serta syscall
      `mprotect` untuk mengubah permission tanpa W+X.
- [~] MEM-15 Accounting per proses, limit 32 MiB, dan OOM kill requester
      saat free frame di bawah reserve. Accounting dan limit diuji lewat
      `hello mem`; jalur OOM kill belum diuji QEMU.
- [~] MEM-16 TLB shootdown lintas core dan ASID reuse yang aman.
      Semua CPU mem-flush TLB lokal saat address space berganti. Perubahan
      permission dan unmap mengirim SGI flush ke CPU online secara
      non-blocking. Ack sinkron dan reuse ASID belum.
- [x] MEM-17 ASLR untuk stack, mmap, dan executable. Aplikasi dynamic
      dibangun sebagai PIE/ET_DYN, kernel memilih load bias acak, dan
      library dimuat di mmap acak. Build statis SDK tetap memakai image
      fixed-base.
- [~] MEM-18 User fault dari EL0 mematikan proses atau ditangani sebagai
      stack growth, bukan panic. COW dan uji fault lain belum.

**Selesai bila:** user tidak bisa mengeksekusi data, dan fork/COW serta
demand paging lulus uji.

## 9. Fase 20 - Scheduler dan sinkronisasi lanjutan

- [ ] SCH-10 Migrasi thread dan work stealing antar core.
- [ ] SCH-11 Prioritas dan nice, plus aging agar tidak starvation.
- [x] SCH-12 Thread join dan reklamasi stack. `ThreadJoin` menunggu thread
      selesai lalu membebaskan stack dan slotnya.
- [x] SCH-13 Condition variable dan read/write lock, diuji lewat self-test
      scheduler.
- [ ] SCH-14 Futex untuk user space.
- [ ] SCH-15 TLS dan dukungan thread-local storage.
- [x] SCH-16 Accounting CPU per proses: tick scheduler menambah `cpu_ticks`
      thread dan `/proc/processes` menampilkan `cpu_ms`.
- [x] SCH-17 Uji stres 4 core: worker per CPU, ping-pong semaphore 200 kali,
      metrik sleep overshoot, dan QEMU `SMP=4`.

**Selesai bila:** beban merata di 4 core, thread bisa join, dan primitif
sinkronisasi lulus uji stres.

## 10. Fase 21 - Platform, boot, dan portabilitas

- [x] PLT-10a RSDP/XSDT discovery, validasi checksum, dan registry tabel.
- [x] PLT-10b MADT cpu count dan GIC distributor/redistributor.
- [x] PLT-10c SPCR UART dan MCFG PCI ECAM.
- [x] PLT-10d FADT flags, PSCI conduit, dan reset register.
- [ ] PLT-10e GTDT detail dan ACPI memory map.
- [ ] PLT-10 ACPI: MADT, GTDT, SPCR, MCFG, dan memory map.
- [ ] PLT-11 DTB: discovery RAM, UART, GIC, timer, PCI, dan device.
- [ ] PLT-12 Platform generik, bukan qemu_virt hardcoded.
- [ ] BOOT-01 initrd, kernel command line, dan modul awal.
- [x] BOOT-02a Boot ACPI dan DTB pada QEMU.
- [ ] BOOT-02 Boot ACPI dan DTB pada QEMU, lalu satu board nyata.
- [ ] BOOT-03 Framebuffer handoff dan konsol awal.
- [x] PLT-13 Shutdown dan reboot terintegrasi dengan shell.
- [x] PLT-14 Matriks boot: 1/2/4 core, memori 256M/512M, ACPI on/off.
      SMP=1 memakai jalur single-core tanpa IPI; QEMU 1/2/4 core dan
      256M/512M lulus.

**Selesai bila:** satu image bisa boot dengan ACPI dan DTB, dan shutdown
bekerja dari shell.

## 11. Fase 22 - Observability, keamanan, dan CI

- [x] OBS-01 Ring buffer dmesg dan level log runtime.
- [x] OBS-02 Counter scheduler, memori, VFS, dan block device serta trace
      ring `/proc/trace` untuk event block, context switch, spawn, dan exit.
- [x] OBS-03 Panic dump menampilkan thread, proses, heap, frame, log bytes,
      dan 8 word stack. SysRq `Ctrl-A` lalu `s`/`b`/`o`/`h` untuk statistik,
      reboot, shutdown, dan halt. Watchdog memeriksa heartbeat tiap CPU.
- [ ] SEC-01 KASLR dan relokasi image.
- [x] SEC-02 PAN dan validasi akses user yang seragam. PAN diaktifkan
      bila CPU mendukung dan diuji dengan QEMU `-cpu max`; cortex-a72
      melaporkan tidak didukung. Semua akses user memakai copy helper
      berbasis translasi fisik.
- [ ] SEC-03 PAC/BTI bila tersedia di target.
- [x] SEC-04 Validasi pointer user diuji dari EL0: `hello badptr` menolak
      pointer invalid, dan `hello fuzz` menjalankan 512 panggilan dengan
      pointer/ukuran acak deterministik tanpa panic. Fuzz coverage-guided
      belum.
- [~] CI-01 GitHub Actions: workflow untuk build native Linux, host test,
      clang-tidy, SDK probe, QEMU smoke DTB/ACPI, dan persistensi. Semua
      langkah lulus lokal; workflow belum dijalankan di GitHub.
- [~] CI-02 Sanitizer host test dan coverage. ASan+UBSan lulus untuk
      seluruh host probe; coverage baris host test inti 58 persen dengan
      ambang 55 persen. Workflow belum dijalankan di GitHub.
- [~] CI-03 Artifact image dan firmware versi terkunci. Workflow mengunggah
      `esp.img`, `BOOTAA64.EFI`, dan `gema-system.img`. Firmware dipin ke
      Debian `qemu-efi-aarch64` 2022.11-6+deb12u2 dengan verifikasi SHA256.
      Workflow belum dijalankan di GitHub.

**Selesai bila:** CI hijau pada setiap push, fuzz syscall tidak memicu
panic, dan hardening user aktif.

## 12. Fase 23 - GUI dan aplikasi (opsional)

- [ ] GUI-01 API gambar, font, dan event input di libkgema.
- [ ] GUI-02 Terminal grafis dengan keyboard dan scrollback.
- [ ] APP-01 Editor teks atau file manager sederhana.
- [ ] GUI-03 Window manager minimal atau compositor.

Fase ini tidak menghalangi v1.0 dan bisa ditunda.

## 13. Urutan yang disarankan

1. Fase 15 dan 16: proses, FD, syscall, SDK, shell. Ini membuka semua
   fitur userland lain.
2. Fase 17: storage dan VFS. Shell butuh berkas yang bisa ditulis.
3. Fase 18 dan 22: driver jaringan/input dan kualitas. Kerjakan CI lebih
   awal jika memungkinkan.
4. Fase 19 dan 20: memori lanjutan dan scheduler. Lakukan setelah ada
   banyak proses agar uji stres masuk akal.
5. Fase 21: platform dan portabilitas.
6. Fase 23: GUI setelah fondasi stabil.

## 14. Definition of done

1. Build bersih dengan `-Werror` dan `check-arch.sh` lolos.
2. Host test dan smoke test QEMU lolos.
3. Ada test baru untuk fitur baru, termasuk kasus gagal.
4. Tidak ada state global baru tanpa alasan yang didokumentasikan.
5. Setiap alokasi punya jalur pembebasan yang diuji.
6. Dokumentasi header, README paket, dan roadmap diperbarui.
7. Tidak ada regresi pada frame bebas, jumlah thread, atau waktu boot
   yang wajar.

## 15. Risiko utama

1. Model proses yang salah akan mahal diperbaiki setelah banyak syscall.
   Kerjakan Fase 15 lebih dulu dan tahan fitur di atasnya.
2. FD global di VFS harus dipindahkan sebelum filesystem writable, bukan
   sesudahnya.
3. TLB shootdown dan migrasi thread saling terkait. Jangan aktifkan
   migrasi sebelum shootdown benar.
4. Driver jaringan bisa menghabiskan banyak waktu. Batasi ke UDP dan
   loopback/QEMU user net dulu.
5. ACPI dan board nyata bisa mengubah asumsi memory map. Pisahkan platform
   discovery dari driver.
6. Jangan menambah exception/RTTI ke kernel atau SDK tanpa runtime yang
   lengkap.

## 16. GEMA System Layer

Arah jangka panjang userland, API, packaging, dan filesystem ada di
`docs/gsl/README.md`.

Ringkasnya:

- Aplikasi hanya boleh memakai `libgemapi`.
- `libgemapi` memakai `libkgema` secara privat.
- Binary dan library dynamic berasal dari filesystem.
- System volume memakai GSLFS read-only.
- Layout Unix-like lowercase: `/system` read-only, `/users` dan `/etc`
  writable, `/dev` dan `/proc` virtual.
- Kernel memuat executable, `ld-gem` memuat library.
- Kernel menjalankan init, init memasang volume dan menjalankan shell.
- Setiap layer punya validasi dan batas kegagalan.

Roadmap GSL ada di `docs/gsl/06-roadmap.md`.

GSL-1 sampai GSL-4 sudah berjalan: `packages/libgemapi` menjadi
satu-satunya API aplikasi, `packages/libgsl` memegang layout tetap, kernel
memuat ELF64 dynamic, dan `ld-gem` memuat library dari `/system/libraries`.
Root `/` adalah volume GSLFS read-only dari partisi kedua. `/dev`, `/proc`,
`/tmp`, `/etc`, `/users`, `/var`, dan `/volumes/boot` sudah di-mount.
`/system/boot/init` berjalan sebagai PID 1 dan menjalankan shell. Shell
mendukung `help`, `echo`, `cat`, `ls`, `clear`, dan aplikasi eksternal di
`/applications/<name>/<name>`. `/etc`, `/users`, dan `/var` masih ramfs
writable sementara; service manager penuh, volume persisten, dan recovery
belum ada.

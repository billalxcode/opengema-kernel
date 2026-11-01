# Roadmap lanjutan opengema-kernel

Status: Fase 0 sampai 14 selesai. Dokumen ini merencanakan fase 15 dan
seterusnya, setelah kernel bisa boot di QEMU virt, menjalankan dua core,
memakai address space per proses, dan menjalankan gemabox di EL0.

## 1. Analisis kesenjangan

Prioritas: P0 blocker, P1 penting, P2 menengah, P3 nanti.

| Area | Kondisi sekarang | Yang kurang | Dampak | Prioritas |
|---|---|---|---|---|
| Model proses | Satu program user, exit memakai flag global | Objek proses, PID, parent, exit status, wait, exec, argv | Belum bisa menjalankan banyak program atau shell | P0 |
| File descriptor | Tabel global 16 slot di VFS | FD per proses, open flags, close, stat, seek | Program tidak bisa membuka berkas sendiri | P0 |
| Syscall | write, read (belum ada sumber), exit, yield, mmap, munmap | read, open, close, stat, seek, getpid, wait, exec, error yang konsisten | Userland hanya bisa uji jalur syscall | P0 |
| Reklamasi | Stack thread, address space, dan FD belum otomatis dibebaskan saat proses keluar | Reklamasi proses, zombie, wait, PID reuse | Membocorkan memori jika ada banyak proses | P0 |
| SDK dan shell | libkgema kecil, gemabox satu program uji | API berkas/proses, argv, konstruktor C++, shell multi perintah, packaging SDK | Belum ada pengalaman userland yang nyata | P1 |
| Filesystem | FAT32 read-only, ramfs statis, mount internal | Tulis berkas, mkdir, unlink, rename, mount/umount, permission, sync | Data tidak bisa disimpan atau diubah | P1 |
| Memori user | Semua halaman user RWX, tanpa demand paging | W^X user, guard stack, stack growth, COW, file-backed mmap, accounting, OOM | Isolasi dan keamanan user lemah | P1 |
| Scheduler | Per-CPU, thread tidak migrasi, tanpa prioritas | Migrasi/work stealing, prioritas, join, condvar, rwlock, futex, TLS | Beban tidak seimbang, primitif sinkronisasi kurang | P1 |
| TLB dan SMP | ASID dipakai, AS dipin ke satu core | TLB shootdown lintas core, ASID reuse yang aman, 4 core test | Mappings kernel/user bisa basi pada SMP | P1 |
| Driver | UART, RTC, PCI, virtio-blk, virtio-gpu | virtio-net, input, MSI-X, virtio IRQ, multi device, DMA streaming | Belum ada jaringan atau input interaktif | P1 |
| Platform | QEMU virt hardcoded, DTB dipakai sebagian | ACPI (MADT/GTDT/SPCR/MCFG), discovery RAM/device, initrd, cmdline | Sulit pindah board atau boot ACPI | P2 |
| Daya dan waktu | RTC dibaca saat boot, halt saja | settime, timezone, PSCI SYSTEM_OFF/RESET, shutdown/reboot | Mesin tidak bisa dimatikan dengan rapi | P2 |
| Observability | Log serial, panic, procfs uptime/threads | dmesg ring, tracing, counter, watchdog, statistik memori/CPU | Debug produksi dan regresi sulit | P1 |
| Test dan CI | Satu smoke test QEMU, host test libs/core | Unit test VMM/sched/FS, matriks QEMU, negative test, fuzzing, sanitizer, CI | Regresi mahal dan mudah lolos | P1 |
| Keamanan | W^X kernel, stack canary | W^X user, KASLR, PAC/BTI, PAN, validasi syscall, fuzzing | Permukaan serangan user ke kernel masih lebar | P1 |
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

- [ ] PRC-01 `Process`: PID, address space, state, exit status, parent,
      daftar anak, FD table, cwd, dan nama program.
- [ ] PRC-02 Pisahkan kernel thread dari proses. Thread punya pointer proses,
      proses punya satu atau lebih thread.
- [ ] PRC-03 Pindahkan tabel FD dari VFS global ke proses. VFS hanya
      menyimpan node; FD menyimpan offset dan flags.
- [ ] PRC-04 Syscall `open`, `close`, `read`, `write`, `stat`, `seek`,
      `getpid`, `exit(status)`, `waitpid`.
- [ ] PRC-05 Loader program dari filesystem: format flat dan ELF64,
      argv/env, susun stack user, entry point dinamis.
- [ ] PRC-06 Lifecycle: zombie, wait, reklamasi stack, address space, FD,
      dan PID. PID tidak boleh dipakai ulang sebelum objek dibersihkan.
- [ ] PRC-07 `spawn` atau `exec` minimal. `fork` bisa menyusul setelah
      COW tersedia.
- [ ] PRC-08 Uji: dua proses berbeda address space, parent wait, exit
      status benar, frame bebas kembali setelah proses selesai.

**Selesai bila:** kernel bisa menjalankan dua program berbeda, parent
menunggu anak, dan tidak ada kebocoran thread, stack, page, atau FD.

## 5. Fase 16 - SDK v2 dan shell gemabox

Tujuan: userland punya API yang layak dan gemabox menjadi shell nyata.

- [ ] SDK-01 `libkgema` API berkas: open, read, write, close, stat, seek.
- [ ] SDK-02 API proses: getpid, wait, exit, spawn, argv.
- [ ] SDK-03 API waktu dan error yang konsisten, tanpa libc.
- [ ] SDK-04 Jalankan `.init_array` di `_start`, dukung konstruktor global
      sederhana tanpa exception dan tanpa RTTI.
- [ ] SDK-05 `_start` meneruskan argc/argv ke `main`.
- [ ] SDK-06 gemabox shell: `help`, `echo`, `ls`, `cat`, `ps`, `sleep`,
      `clear`, `exit`, dengan tabel perintah.
- [ ] SDK-07 Input UART ke TTY: line buffer, backspace, echo, read blocking.
- [ ] SDK-08 Packaging: install header dan `libkgema.a`, CMake package
      config, satu contoh program baru, dan dokumentasi link.
- [ ] SDK-09 Uji: shell menerima perintah, `cat` membaca berkas, `ps`
      menampilkan proses, `echo` menulis lewat FD.

**Selesai bila:** shell interaktif berjalan di atas UART dan perintah dasar
bisa dijalankan tanpa menyalin kode syscall.

## 6. Fase 17 - Storage dan VFS

- [ ] FS-10 FD dan open flags per proses, termasuk read/write/append.
- [ ] FS-11 Operasi directory: mkdir, rmdir, unlink, rename, lookup.
- [ ] FS-12 ramfs dinamis: node dan data dialokasikan serta dibebaskan.
- [ ] FS-13 Filesystem writable. Pilih satu: FAT32 write atau filesystem
      sederhana dengan journal ringan. Dokumentasikan pilihannya.
- [ ] FS-14 mount/umount syscall dan mount table per proses atau global
      dengan referensi.
- [ ] FS-15 Block layer: IRQ/MSI-X, flush, multiple device, partisi.
- [ ] FS-16 Metadata: permission, uid/gid, timestamp, ukuran.
- [ ] FS-17 Uji persistensi: buat berkas, tulis, reboot, baca kembali.

**Selesai bila:** berkas bisa dibuat dan diubah, lalu tetap ada setelah
reboot.

## 7. Fase 18 - Driver framework, jaringan, dan input

- [ ] DRV-10 Device resource: MMIO, IRQ, DMA, clock, dan probe terurut.
- [ ] DRV-11 Virtio IRQ/MSI-X, completion tanpa polling.
- [ ] DRV-12 virtio-net: transmit/receive, queue, interrupt.
- [ ] NET-01 Ethernet, ARP, IPv4, ICMP echo.
- [ ] NET-02 UDP dan socket API minimal.
- [ ] NET-03 QEMU user networking dan uji echo.
- [ ] DRV-13 virtio input keyboard dan mouse, event queue.
- [ ] DRV-14 TTY input dari keyboard, bukan hanya UART.
- [ ] TIME-06 RTC settime, zona waktu, dan waktu dinding.
- [ ] OPS-01 shutdown/reboot lewat PSCI SYSTEM_OFF dan SYSTEM_RESET.

**Selesai bila:** kernel menjawab ping/UDP dari host dan menerima input
keyboard di jendela QEMU.

## 8. Fase 19 - Memori lanjutan dan isolasi

- [ ] MEM-10 Demand paging dan page fault handler yang aman.
- [ ] MEM-11 COW untuk fork atau clone.
- [ ] MEM-12 Stack growth dan guard page user.
- [ ] MEM-13 File-backed mmap dan shared memory.
- [ ] MEM-14 W^X user: code RX, data/stack RW dan NX.
- [ ] MEM-15 Accounting per proses, limit, dan OOM killer minimal.
- [ ] MEM-16 TLB shootdown lintas core dan ASID reuse yang aman.
- [ ] MEM-17 ASLR untuk stack, mmap, dan executable.
- [ ] MEM-18 Uji: fault invalid, COW, limit memori, dan isolasi.

**Selesai bila:** user tidak bisa mengeksekusi data, dan fork/COW serta
demand paging lulus uji.

## 9. Fase 20 - Scheduler dan sinkronisasi lanjutan

- [ ] SCH-10 Migrasi thread dan work stealing antar core.
- [ ] SCH-11 Prioritas dan nice, plus aging agar tidak starvation.
- [ ] SCH-12 Thread join dan reklamasi stack.
- [ ] SCH-13 Condition variable dan read/write lock.
- [ ] SCH-14 Futex untuk user space.
- [ ] SCH-15 TLS dan dukungan thread-local storage.
- [ ] SCH-16 Accounting CPU per proses dan statistik di procfs.
- [ ] SCH-17 Uji stres 4 core, latency preemption, dan lost wakeup.

**Selesai bila:** beban merata di 4 core, thread bisa join, dan primitif
sinkronisasi lulus uji stres.

## 10. Fase 21 - Platform, boot, dan portabilitas

- [ ] PLT-10 ACPI: MADT, GTDT, SPCR, MCFG, dan memory map.
- [ ] PLT-11 DTB: discovery RAM, UART, GIC, timer, PCI, dan device.
- [ ] PLT-12 Platform generik, bukan qemu_virt hardcoded.
- [ ] BOOT-01 initrd, kernel command line, dan modul awal.
- [ ] BOOT-02 Boot ACPI dan DTB pada QEMU, lalu satu board nyata.
- [ ] BOOT-03 Framebuffer handoff dan konsol awal.
- [ ] PLT-13 Shutdown dan reboot terintegrasi dengan shell.
- [ ] PLT-14 Matriks boot: 1/2/4 core, memori kecil, ACPI on/off.

**Selesai bila:** satu image bisa boot dengan ACPI dan DTB, dan shutdown
bekerja dari shell.

## 11. Fase 22 - Observability, keamanan, dan CI

- [ ] OBS-01 Ring buffer dmesg dan level log runtime.
- [ ] OBS-02 Tracepoint, counter, dan statistik scheduler/memori/FS.
- [ ] OBS-03 Watchdog, panic dump lengkap, dan magic SysRq minimal.
- [ ] SEC-01 KASLR dan relokasi image.
- [ ] SEC-02 PAN dan validasi akses user yang seragam.
- [ ] SEC-03 PAC/BTI bila tersedia di target.
- [ ] SEC-04 Validasi syscall dan fuzzing argumen user.
- [ ] CI-01 GitHub Actions: build, host test, QEMU smoke, clang-tidy.
- [ ] CI-02 Sanitizer host test dan coverage.
- [ ] CI-03 Artifact image dan firmware versi terkunci.

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

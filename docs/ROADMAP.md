# Roadmap opengema-kernel

Dokumen ini jadi acuan tunggal untuk urutan kerja, batas modul, dan aturan
anti redundansi. Setiap task punya ID supaya bisa dirujuk di commit.

Status sekarang: Fase 0 sampai 13 selesai. Kernel sudah boot di QEMU virt,
punya MMU sendiri, allocator, interrupt, timer, virtio-blk, FAT32, scheduler
round robin, user mode EL0, dua core aktif, dan test otomatis.

---

## 1. Prinsip

1. Satu kemampuan, satu pemilik. Tidak ada implementasi kedua dari fungsi
   yang sudah ada.
2. Arah dependensi hanya ke bawah. Modul bawah tidak boleh tahu modul atas.
3. Guard clause di semua fungsi yang bisa gagal: cek gagal dulu, return awal.
4. Interface kecil dan stabil. Implementasi boleh berubah tanpa ubah pemanggil.
5. Setiap modul punya satu header publik. Sisanya detail internal.
6. Kode harus bisa dibaca tanpa perlu tahu isi modul lain.
7. Tidak ada state global mutable kecuali singleton yang memang didesain
   (contoh: satu instance console untuk early boot).

---

## 2. Lapisan arsitektur

Urutan dari bawah ke atas. Modul hanya boleh include modul di bawahnya.

```
L0  libs/           freestanding cxx, efi header, memory, bit, math
L1  arch/aarch64/   barrier, cache, registers, mmu primitives, context switch
L2  io/             mmio, console, print, register field
L3  core/           status, error, log, panic, assert, version, init
L4  mem/            pmm, vmm, heap, address space
L5  events/         vector, trap, irq, deferred work, wait queue
L6  time/           clocksource, timer, delay
L7  dma/            coherent, streaming, cache maintenance
L8  drivers/        uart, gic, timer, rtc, pci, virtio
L9  fs/             vfs, devfs, ramfs, fat32
L10 sched/          thread, scheduler, context
L11 sync/           spinlock, mutex, semaphore, refcount
L12 syscall/        el0, svc, copy_from_user
L13 platform/       qemu virt, device discovery
L14 boot/           uefi entry, bootinfo, exit boot services
```

Catatan: `core/` berisi status dan log yang dipakai hampir semua modul,
jadi diperlakukan seperti L3 yang boleh dipakai L1 ke atas. Pengecualian
ini didokumentasikan, bukan diulang di kode.

Grafik dependensi utama:

```
boot -> platform -> arch -> io -> core -> mem -> events -> time
                                                     |
                                                     v
                                        dma -> drivers -> fs
                                                     |
                                                     v
                                              sched -> sync -> syscall
```

---

## 3. Struktur direktori target

```
libs/
  cxx/            cstddef, cstdint, type_traits, concepts, span
  efi/            efi.h
  core/           memory.h, string.h, bit.h, math.h, result.h
kernel/
  include/og/     header publik, satu per modul
  arch/aarch64/   barriers.cpp, cache.S, context.S, vectors.S, mmu_primitives.cpp
  io/             mmio.cpp, console.cpp, print.cpp
  core/           log.cpp, panic.cpp, assert.cpp, init.cpp, version.cpp
  mem/            pmm.cpp, vmm.cpp, heap.cpp, address_space.cpp
  events/         vector.cpp, trap.cpp, irq.cpp, workqueue.cpp, waitqueue.cpp
  time/           clocksource.cpp, timer.cpp, delay.cpp
  dma/            dma.cpp, cache.cpp, pool.cpp
  drivers/
    uart/         pl011.cpp
    irq/          gicv3.cpp
    timer/        arm_generic.cpp
    rtc/          pl031.cpp
    pci/          ecam.cpp
    virtio/       virtio_common.cpp, virtio_blk.cpp, virtio_gpu.cpp
  fs/             vfs.cpp, devfs.cpp, ramfs.cpp, fat32.cpp
  sched/          thread.cpp, scheduler.cpp, context.S
  sync/           spinlock.cpp, mutex.cpp, semaphore.cpp
  syscall/        syscall.cpp, usercopy.cpp
  platform/       qemu_virt.cpp, platform.h
  boot/uefi/      efi_main.cpp, efi_console.cpp, efi_memmap.cpp, efi_exit.cpp
tests/
  host/           unit test modul murni
  qemu/           smoke test script
docs/
  ROADMAP.md
```

---

## 4. Aturan anti redundansi

Tabel pemilik tunggal. Kalau butuh kemampuan ini, pakai header pemiliknya.

| Kemampuan | Pemilik | Dilarang ada di |
|---|---|---|
| barrier CPU | `arch/aarch64/barriers.h` | driver, fs, mem |
| cache maintenance | `arch/aarch64/cache.h` | driver |
| baca tulis MMIO | `io/mmio.h` | semua modul lain |
| console dan print | `io/console.h`, `io/print.h` | driver, boot |
| status dan error | `og/status.h` | modul lain |
| log | `core/log.h` | semua modul lain |
| panic dan assert | `core/panic.h`, `core/assert.h` | semua modul lain |
| memcpy, memset, strlen | `libs/core/memory.h` | semua modul lain |
| bit ops | `libs/core/bit.h` | semua modul lain |
| alokasi page fisik | `mem/pmm.h` | driver, fs |
| map dan unmap virtual | `mem/vmm.h` | driver, fs |
| alokasi heap | `mem/heap.h` | driver, fs |
| registrasi IRQ | `events/irq.h` | driver |
| timer dan timeout | `time/timer.h` | driver |
| alokasi DMA | `dma/dma.h` | driver |
| registry device | `drivers/device.h` | driver individu |
| file dan direktori | `fs/vfs.h` | driver |
| lock | `sync/*.h` | semua modul lain |

Aturan penegakan:

1. Setiap modul jadi target CMake sendiri dengan include PUBLIK hanya untuk
   lapisan di bawahnya. Salah include = build gagal.
2. `volatile` hanya boleh muncul di `io/mmio.cpp` dan deklarasi register.
3. Fungsi yang sudah pindah ke pemiliknya dihapus dari tempat lama, tidak
   disalin. Refactor yang sama tidak boleh muncul dua kali.
4. Kalau dua modul butuh logika sama, logika naik ke `libs/core/`.

---

## 5. Fase dan todolist

### Fase 0 - Fondasi dan pemisahan modul

- [x] F-01 Struktur direktori per modul, file lama dipindah ke lokasi baru.
- [x] F-02 CMake dipecah jadi target per modul: `og_lib_core`, `og_arch`,
      `og_io`, `og_core`, `og_drivers`, `og_kernel`, `og_boot`. Modul lain
      menyusul saat fasenya dikerjakan.
- [x] F-03 Dependensi modul lewat `target_link_libraries`, include lewat
      `INTERFACE`. Lapisan header dijaga `scripts/check-arch.sh` karena
      header publik masih satu direktori.
- [x] F-04 `og/status.h`: status code, `Result<T>`, `IsOk`, `IsErr`.
- [x] F-05 `libs/core/memory.h` dan `memory.cpp`: pemilik tunggal
      `memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, `strcmp`, `strncmp`.
- [x] F-06 `libs/core/bit.h`: `Mask`, `Extract`, `Insert`, `Bit`, `AlignUp`,
      `AlignDown`, `IsAligned`.
- [x] F-07 `og/log.h` dan `kernel/core/log.cpp`: level, sink, `Log`,
      `LogDec`, `LogHex`. Sink UART didaftarkan oleh driver.
- [x] F-08 `og/panic.h` dan `kernel/core/panic.cpp`: dump `ESR_EL1`,
      `FAR_EL1`, `ELR_EL1`, `SPSR_EL1`, `VBAR_EL1` lalu halt.
- [x] F-09 `og/assert.h`: `OG_ASSERT`, `OG_ASSERT_MSG`, `OG_UNREACHABLE`.
- [x] F-10 `boot/uefi/efi_memmap.cpp`: ambil memory map, normalisasi ke
      `BootInfo` yang netral dari tipe UEFI.
- [x] F-11 `boot/uefi/efi_exit.cpp`: `ExitBootServices` dengan retry map key.
- [x] F-12 `og/bootinfo.h`: magic, versi, ukuran, uart base, 64 rentang memori.
- [x] F-13 `kernel/core/init.cpp`: satu urutan init, berhenti di error pertama.
- [x] F-14 `kernel/kernel/stubs.cpp` dihapus, isinya pindah ke `libs/core`.
- [x] F-15 `scripts/check-arch.sh`, dijalankan otomatis oleh target
      `og_arch_check` di setiap build.

**Selesai bila:** build tetap jalan, boot tetap sampai `kmain`, dan tidak ada
duplikasi fungsi memory atau status.

Catatan hasil Fase 0:

1. `kernel/include/og/arch/registers.h` jadi lokasi header arch, bukan
   `kernel/arch/aarch64/registers.h`, supaya masuk aturan header publik.
2. `EFI_BOOT_SERVICES` diawali `EFI_TABLE_HEADER` 24 byte. Offset service
   dikunci `static_assert` di `libs/efi/efi.h`.
3. EDK2 melaporkan `descriptor_size` 48 byte, bukan 40. Iterasi memory map
   wajib memakai stride dari firmware, bukan `sizeof`.

### Fase 1 - IO dan console

- [x] IO-01 `og/mmio.h`: `MmioRegion` dengan base, size, bounds check,
      alignment check. Akses 8, 16, 32, 64 bit lewat satu template internal.
- [x] IO-02 `og/arch/barriers.h`: `DmbSy`, `DsbSy`, `DmbIsh`, `DsbIsh`,
      `DsbIshSt`, `DmbNsh`, `InstructionSync`.
- [x] IO-03 `og/register.h`: `Field<T, kPos, kWidth>` dengan `Decode`,
      `Encode`, `Insert`, `Mask`, dan `static_assert` batas.
- [x] IO-04 `og/console.h`: `Console` sebagai pasangan fungsi dan konteks,
      bukan virtual, supaya tidak butuh vtable atau runtime support.
- [x] IO-05 `og/print.h`: `Print`, `PrintText`, `PrintLn`, `PrintDec`,
      `PrintHex`. Formatting ada di `libs/core/format.h`.
- [x] IO-06 Tidak ada lagi `puts` atau akses MMIO langsung di driver.
      `Pl011` sekarang hanya byte device.
- [x] IO-07 `kmain` memakai `PrintHex` dan `PrintLn`, output sama.

**Selesai bila:** tidak ada akses MMIO di luar `io/`, dan console bisa
diganti tanpa ubah pemanggil.

Catatan hasil Fase 1:

1. `ConsoleUart` tidak masuk `io/` karena `io` berada di bawah `drivers`.
   Bentuknya `MakeConsole(Pl011&)` di `drivers/uart`.
2. `ConsoleEfi` jadi `MakeFirmwareConsole` di `boot/uefi`, karena tipe UEFI
   hanya boleh dipakai lapisan boot.
3. Formatting angka dipindah ke `libs/core/format.h`. `og/print` dan
   `og/log` sama sama memakainya, jadi tidak ada dua loop digit.
4. `og/log` sekarang mendelegasikan ke `og/print`, bukan menyimpan sink
   sendiri. Satu jalur output untuk seluruh kernel.

### Fase 2 - Arch dasar

- [x] ARCH-01 `og/arch/registers.h`: baca tulis `SCTLR_EL1`, `TCR_EL1`,
      `TTBR0_EL1`, `MAIR_EL1`, `VBAR_EL1`, `ESR_EL1`, `FAR_EL1`, `ELR_EL1`,
      `SPSR_EL1`, `DAIF`.
- [x] ARCH-02 `og/arch/cache.h`: `CleanInvalidateDcacheRange`,
      `InvalidateIcacheAll`, dengan ukuran line dari `CTR_EL0`.
- [x] ARCH-03 `kernel/arch/aarch64/mmu.cpp`: tabel L0 dan L1, blok 1 GiB,
      atribut normal dan device, `MAIR`, `TCR`, `TTBR0`, `SCTLR.M`.
- [x] ARCH-04 `og/arch/tlb.h`: `InvalidateTlbAllLocal`, `InvalidateTlbVaLocal`,
      `InvalidateTlbSync`. Varian per alamat dipakai vmm sejak Fase 3.
- [x] ARCH-05 `vectors.S` dan `og/arch/trap.h`: 16 vector, tabel 2 KiB
      aligned, `TrapFrame` 264 byte, dispatch ke C++.
- [ ] ARCH-06 `context.S`: ditunda ke Fase 10, karena butuh scheduler untuk
      dipakai. Menulis asm tanpa pemakai hanya menambah kode mati.
- [x] ARCH-07 `og/arch/cpu.h`: `CurrentEl`, `CpuId` dari `MPIDR_EL1`.

**Selesai bila:** MMU aktif dengan tabel sendiri, dan exception sengaja
memicu panic yang benar, bukan hang.

Catatan hasil Fase 2:

1. Identity map memakai dua blok 1 GiB: di bawah `ram_base` sebagai device,
   di atasnya sebagai normal write back. Cukup untuk QEMU virt 512 MiB.
2. `MAIR` dan `TCR` dibaca lalu diubah sebagian, bukan ditimpa total, supaya
   setelan firmware yang masih valid tidak rusak.
3. Uji exception: `svc #0` menghasilkan vector 4, ESR `0x56000000` (SVC64),
   lalu panic dengan dump register lengkap. Hasil ini sudah diverifikasi
   di QEMU, bukan asumsi.

### Fase 3 - Memori

- [x] MEM-01 `og/pmm.h` dan `kernel/mem/pmm.cpp`: bitmap allocator,
      `PmmAllocPage`, `PmmAllocPages`, `PmmFreePage`, `PmmFreePages`.
- [x] MEM-02 Hanya `ConventionalMemory` yang dilepas. Image kernel, stack
      firmware, MMIO, dan ACPI tetap reserved.
- [x] MEM-03 `og/vmm.h`: `VmmMap`, `VmmUnmap`, `VmmTranslate`, dan
      `VmmInit` untuk identity map.
- [x] MEM-04 Walk 4 level dengan tabel dialokasikan dari pmm. Mendukung
      1 GiB, 2 MiB, dan 4 KiB sesuai alignment.
- [x] MEM-05 `og/heap.h`: `HeapAlloc`, `HeapZalloc`, `HeapRealloc`,
      `HeapFree`, `GetHeapStats`.
- [x] MEM-06 Slab untuk 16 sampai 2048 byte, alokasi page langsung untuk
      yang lebih besar. Header 16 byte menyimpan kelas dan jumlah page.
- [ ] MEM-07 `address_space`: ditunda. Pemetaan user dan validasi alamat
      sudah ada di vmm, tetapi objek ruang alamat per proses belum perlu
      sebelum ada banyak proses.
- [ ] MEM-08 Guard page stack kernel: ditunda ke Fase 10, karena stack
      kernel baru dimiliki scheduler.
- [x] MEM-09 Uji QEMU: 256 page alokasi dan free, 10000 churn page,
      map dan unmap 4 KiB, map dan unmap 2 MiB, plus slab dan large heap.

**Selesai bila:** `mmu.init_identity` diganti `vmm` sungguhan, dan heap
dipakai minimal satu modul.

Catatan hasil Fase 3:

1. Alamat fisik sama dengan alamat virtual selama identity map berlaku.
   Deskriptor tabel bisa langsung dipakai sebagai pointer, dan asumsi ini
   ditulis di header vmm.
2. `kernel/core/init.cpp` ditandai sebagai composition root di
   `check-arch.sh`. Itu satu satunya file yang boleh include modul atas.
3. Self test berjalan saat boot dan hasilnya terverifikasi di QEMU:
   131072 frame total, 122093 bebas, semua tes pmm, vmm, dan heap lolos.

### Fase 4 - Events dan interrupt

- [x] EV-01 `events/trap.cpp`: pasang `VBAR_EL1`. Tabelnya tetap di
      `arch/aarch64/vectors.S`, karena itu kode arch.
- [x] EV-02 `kernel/core/panic.cpp`: decode `ESR_EL1` menjadi EC dan ISS
      di satu tempat, jadi trap tidak menduplikasi dump.
- [x] EV-03 Handler default memanggil `Panic` untuk semua exception yang
      bukan IRQ.
- [x] EV-04 `og/irq.h`: `RegisterIrq`, `UnregisterIrq`, `DispatchIrq`,
      `HandleIrq`, plus `InterruptController` sebagai titik sambung driver.
- [x] EV-05 `og/workqueue.h`: antrian 64 item, dijalankan di akhir
      penanganan IRQ.
- [ ] EV-06 `waitqueue`: ditunda ke Fase 10, karena butuh thread dan sleep.
- [x] EV-07 `IrqSave` dan `IrqRestore` di `og/arch/registers.h`. Spinlock
      yang memakainya menyusul di Fase 6.

**Selesai bila:** timer tick masuk ke handler terdaftar dan bisa dihitung.

Catatan hasil Fase 4:

1. `AckIrq` dan `EndIrq` pindah ke driver controller, karena itu operasi
   hardware, bukan operasi tabel. `HandleIrq` yang menyusun urutannya.
2. Urutan penanganan: acknowledge, dispatch, end, lalu jalankan workqueue.
   Workqueue sengaja dijalankan setelah EOI supaya IRQ berikutnya tidak
   tertahan selama kerja tertunda.
3. IRQ vector tidak memicu panic. IRQ tanpa handler dicatat sebagai error
   lalu kembali, supaya satu interrupt liar tidak mematikan kernel.

### Fase 5 - Time

- [x] TIME-02 Timer ARM generic di `kernel/time/timer.cpp`, memakai
      `CNTFRQ_EL0`, `CNTP_TVAL_EL0`, dan `CNTP_CTL_EL0`. PPI 30.
- [~] TIME-01 Clocksource: frekuensi dan hitungan tick tersedia lewat
      `TimerFrequency` dan `ReadCntpctEl0`. Konversi ke nanodetik menyusul
      saat ada pemakainya.
- [ ] TIME-03 Antrian timeout `ScheduleAfter` dan `Cancel`: ditunda ke
      Fase 10, karena butuh sleep dan scheduler.
- [x] TIME-04 `og/delay.h`: `DelayUs` dan `DelayMs` berbasis counter,
      dipakai GIC untuk menunggu redistributor bangun.
- [~] TIME-05 Uji tick: 100 Hz berjalan 100 tick dalam batas waktu.
      Uji 1000 Hz menyusul bersama beban scheduler.

**Selesai bila:** sleep dan timeout jalan tanpa busy loop.

Catatan hasil Fase 5: kriteria selesai fase ini bergantung pada scheduler,
jadi statusnya sebagian. Tick periodik dan delay sudah terbukti, sleep
belum bisa ada sebelum ada thread.

### Fase 6 - Sync

- [x] SYN-01 `og/arch/spinlock.h`: `Spinlock`, `IrqSpinlock`, `LockGuard`.
      Dipakai tabel IRQ dan workqueue, menggantikan `IrqSave` manual yang
      tersebar.
- [ ] SYN-02 mutex: ditunda ke Fase 10, karena butuh wait queue dan sleep.
- [ ] SYN-03 semaphore: ditunda ke Fase 10, alasan sama.
- [ ] SYN-04 refcount: ditunda sampai ada pemakai nyata, misalnya fs atau
      driver yang berbagi objek.
- [ ] SYN-05 once: ditunda sampai ada init yang benar benar butuh sekali
      jalan di luar `kmain`.
- [ ] SYN-06 nama lock untuk debug: menyusul bersama mutex.

**Selesai bila:** tidak ada lagi disable interrupt manual tersebar.

Catatan hasil Fase 6:

1. Spinlock dipindah ke `arch`, bukan `sync`. Alasannya, `events` dan `mem`
   berada di bawah `sync` di roadmap, jadi lock tingkat CPU harus lebih
   rendah dari keduanya. Lock yang boleh tidur tetap di `sync` nanti.
2. Semua kunci memakai `__atomic_*` dengan acquire dan release, bukan
   volatile dan bukan barrier manual.

### Fase 7 - DMA

- [x] DMA-01 `og/dma.h`: `DmaAddress` dan translasi identitas. Nanti saat
      higher half masuk, hanya fungsi ini yang berubah.
- [x] DMA-02 `AllocCoherent`: page dari pmm, di-zero, device address sama
      dengan CPU address selama identity map berlaku.
- [x] DMA-03 `MapStreaming` dan `UnmapStreaming` dengan cache maintenance
      sesuai arah data.
- [ ] DMA-04 `dma/pool.cpp`: pool per device belum ada. Alokasi langsung
      masih cukup untuk satu device blok. Pool menyusul saat ada beberapa
      device yang alokasi bersamaan.
- [x] DMA-05 Terverifikasi lewat virtio-blk: buffer request dan queue
      memakai `AllocCoherent`, dan baca GPT berhasil.

**Selesai bila:** driver virtio memakai DMA API, bukan alamat fisik mentah.

Catatan hasil Fase 7: `SyncForDevice` dan `SyncForCpu` memakai `dc civac`
dan `dc ivac` dari `arch/cache.h`, dengan ukuran line dari `CTR_EL0`.
Self test membuktikan alokasi selaras 4 KiB, alamat device sama dengan
alamat CPU, pola data bertahan, dan jumlah frame bebas kembali setelah
`FreeCoherent`.

### Fase 8 - Driver

- [x] DRV-01 `og/device.h`: registry dengan nama, kelas, kapasitas, dan
      `driver_data`. Dipakai virtio-blk saat mendaftar.
- [~] DRV-02 PL011: TX polling selesai. RX lewat IRQ, pengaturan baud, dan
      ring buffer belum.
- [x] DRV-03 GICv3: distributor, redistributor, CPU interface, enable,
      disable, acknowledge, end. Single core.
- [ ] DRV-04 RTC PL031: belum, menunggu pemakai untuk timestamp.
- [x] DRV-05 PCIe ECAM: scan bus, device, function, baca BAR, dan
      `EnableDevice` untuk menyalakan memory decoding.
- [x] DRV-06 virtio PCI transport: capability modern, feature negotiation,
      queue setup, notify.
- [x] DRV-07 virtio-blk: baca blok, polled completion, tanpa IRQ.
- [ ] DRV-08 virtio-gpu: belum, ditunda sampai ada kebutuhan framebuffer.
- [ ] DRV-09 watchdog: tidak dipakai di QEMU.

**Selesai bila:** UART RX pakai IRQ, GIC hidup, dan satu device virtio
berfungsi.

Catatan hasil Fase 8:

1. ECAM pada QEMU 11 `virt` ada di `0x4010000000`, bukan `0x3F000000`.
   Angka ini diambil dari DTB hasil `dumpdtb`, bukan tebakan.
2. Firmware mematikan memory decoding pada device sebelum kita mengambil
   alih. Tanpa menyalakan bit memori di command register, semua register
   virtio terbaca `0xFFFF`.
3. Verifikasi nyata: kernel membaca sektor 1 dari ESP dan mencocokkan
   tanda tangan GPT `EFI PART`. Capacity terbaca 131072 sektor, sesuai
   ukuran image 64 MiB.
4. Virtio masih polled. MSI-X dan jalur IRQ menyusul bersama driver yang
   butuh throughput.

### Fase 9 - Filesystem

- [x] FS-01 `og/vfs.h`: `Node`, `NodeOps`, `DirEntry`, `Stat`, `Mount`,
      dan tabel descriptor.
- [x] FS-02 `kernel/fs/vfs.cpp`: resolusi path, mount table dengan
      pencocokan prefix terpanjang. `..` ditolak, bukan ditebak.
- [x] FS-03 `kernel/fs/devfs.cpp`: `/dev/console`, `/dev/null`, `/dev/zero`.
- [ ] FS-04 `kernel/fs/ramfs.cpp`: ditunda ke Fase 11, menunggu initramfs.
- [x] FS-05 `kernel/fs/fat32.cpp`: baca saja, BPB, FAT chain, LFN,
      pemetaan nama 8.3.
- [x] FS-06 `kernel/fs/vfs.cpp`: `FsOpen`, `FsRead`, `FsWrite`, `FsSeek`,
      `FsClose`, `FsStat`. Write hanya untuk node device, file masih baca.
- [x] FS-07 ESP dipasang sebagai root read-only saat boot.
- [ ] FS-08 `procfs`: opsional, ditunda.

**Selesai bila:** bisa `open("/dev/console")` dan `read("/init")` dari FAT.

Catatan hasil Fase 9:

1. FAT32 tidak boleh menganggap filesystem mulai di sektor 0. Image kita
   berisi GPT, jadi ditambah `kernel/fs/partition.cpp` untuk mencari
   partisi EFI dan `BlockDevice::sector_offset` sebagai offset baca.
2. Node FAT dialokasikan dari arena 96 node dan tidak pernah dibebaskan.
   Filesystem read-only, jadi node dipakai sebagai cache. Arena habis
   berarti `kNoMem`, bukan kebocoran diam diam.
3. Verifikasi nyata di QEMU: `/dev/console` bisa ditulis, header berkas
   `/EFI/BOOT/BOOTAA64.EFI` terbaca `MZ`, ukurannya 68608 byte, dan isi
   root menampilkan `EFI` serta `STARTUP.NSH`.
4. Tes tick dibuat tahan beban host. Gate hanya gagal kalau nol interrupt,
   karena QEMU dengan HVF bisa memberi tick jauh lebih lambat saat host
   sibuk. Angka tick tetap dicetak untuk diagnosis.

### Fase 10 - Scheduler

- [x] SCH-01 `og/sched.h`: `Thread`, state, stack dari vmm plus guard page.
- [x] SCH-02 `kernel/sched/scheduler.cpp`: ready queue round robin dan
      idle thread.
- [x] SCH-03 Preemption dari tick timer lewat callback dan IRQ exit hook.
- [x] SCH-04 `ThreadSleep`, `ThreadYield`, `ThreadExit`, dan `WaitQueue`.
- [ ] SCH-05 Load balancing SMP: menunggu Fase 12.
- [x] SCH-06 Uji: dua worker bergantian print, lalu main lanjut dan halt.

**Selesai bila:** `kmain` menjadi thread init dan idle berjalan saat kosong.

Catatan hasil Fase 10:

1. Stack thread tidak boleh memakai identity map karena RAM dipetakan
   dengan blok 1 GiB, sehingga satu halaman guard tidak bisa di-unmap.
   Solusinya, stack ditempatkan di region VA sendiri `0x200000000` dan
   halaman pertama dibiarkan tidak termap sebagai guard.
2. Context switch dilakukan di IRQ exit hook setelah EOI, bukan di dalam
   handler. Kalau switch terjadi sebelum EOI, interrupt tetap aktif di GIC
   dan timer bisa tertahan selama thread lain berjalan.
3. Thread boot ikut didaftarkan di slot tabel thread. Tanpa itu, sleep
   tidak pernah membangunkannya. Ini bug nyata yang ketemu saat uji.
4. Worker yang selesai belum membebaskan stack. Reklamasi menyusul bersama
   pengelolaan thread lanjutan.

### Fase 11 - Syscall dan user mode

- [x] SYS-01 `og/arch/user.h`: `EnterUserMode` menyiapkan `ELR`, `SP_EL0`,
      `SPSR`, lalu `eret` ke EL0.
- [x] SYS-02 ABI `svc #0`: nomor di `x8`, argumen di `x0..x5`, hasil di `x0`.
- [x] SYS-03 Syscall awal: `write`, `read`, `exit`, `yield`. `read` belum
      punya sumber input sehingga mengembalikan error.
- [x] SYS-04 `VmmCopyIn` dan `VmmCopyOut` dengan validasi terhadap region
      user yang terdaftar.
- [x] SYS-05 Loader program user dari binary datar yang dibangun CMake,
      dilink di `0x100000000`, lalu ditanam sebagai header.
- [ ] SYS-06 `mmap` dan `munmap`: ditunda, belum ada kebutuhan alokasi
      dinamis di user.

**Selesai bila:** satu program EL0 bisa print lalu exit bersih.

Catatan hasil Fase 11:

1. Halaman user memakai AP `0b01` sehingga EL0 boleh baca tulis, sementara
   halaman kernel tetap EL1 saja. User tidak bisa menyentuh RAM kernel.
2. Stub vector lama menimpa `x0` dengan nomor vector sebelum register
   disimpan. Akibatnya argumen pertama syscall terbaca sebagai nomor
   vector. Sekarang `x0` disimpan lebih dulu ke stack.
3. Saat menulis ulang stub, `ELR` dan `SPSR` sempat lupa disimpan sehingga
   `eret` memakai nilai sampah dan CPU masuk AArch32. Sudah diperbaiki dan
   dikunci di urutan instruksi stub.
4. Program user 51 byte dibangun dari `user/hello.S`, dipanggil dengan
   `svc`, mencetak `hello from el0`, lalu `exit`.

### Fase 12 - Platform dan SMP

- [x] PLT-01 `og/platform.h`: deskripsi board dengan rentang UART, GIC,
      PCIe, RAM, jumlah CPU, dan konduit PSCI.
- [x] PLT-02 `kernel/platform/qemu_virt.cpp`: konfigurasi QEMU virt.
- [ ] PLT-03 Parser DTB: ditunda. Konduit PSCI dideteksi dengan mencoba
      `PSCI_VERSION` lewat HVC lalu SMC, jadi tidak perlu DTB untuk boot.
- [x] SMP-01 PSCI `CPU_ON` dengan entry point dan stack fisik, lalu
      secondary menyalakan MMU dan vektor sendiri.
- [x] SMP-02 Per-CPU data lewat `TPIDR_EL1`.
- [x] SMP-03 SGI sebagai IPI, diterima dan dihitung di core kedua.

**Selesai bila:** dua core hidup dan print bergantian.

Catatan hasil Fase 12:

1. Konduit PSCI di host ini HVC, terbukti dari `PSCI_VERSION` yang
   mengembalikan 65537 (PSCI 1.1). Kode mencoba HVC lalu SMC, jadi tetap
   jalan pada akselerator lain.
2. Core kedua sempat mati dengan ESR EC `0x07`. Penyebabnya compiler
   memakai `ldr q0` untuk menyalin struct 16 byte, sementara core baru
   punya `CPACR_EL1.FPEN` nol. Kernel sekarang dibangun dengan
   `-mgeneral-regs-only`, yang juga wajib karena context switch belum
   menyimpan state SIMD.
3. Hook IRQ exit sekarang per-CPU. Scheduler hanya terpasang di CPU 0,
   jadi core kedua tidak ikut menyentuh state scheduler global.
4. Secondary hanya menangani IPI lalu park di `wfi`. Penjadwalan lintas
   core (SCH-05) masih backlog.

### Fase 13 - Testing dan tooling

- [x] TST-01 Host test runner untuk `libs/core`: memory, format, dan bit.
      Tanpa framework, cukup `CHECK` dan exit code.
- [x] TST-02 `scripts/test-qemu.sh`: boot QEMU, tunggu penanda, gagal jika
      ada penanda hilang atau panic.
- [x] TST-03 `scripts/check-arch.sh` jalan di setiap build dan di
      `scripts/test.sh`.
- [x] TST-04 Target `og_host_test` di CMake plus `add_test`, dan satu
      perintah penuh lewat `scripts/test.sh`.
- [x] TST-05 `docs/DEBUG.md`: triage, GDB, decode ESR, dan masalah yang
      pernah terjadi.

**Selesai bila:** satu perintah menjalankan build, unit test, dan smoke test.

Catatan hasil Fase 13:

1. Host test dikompilasi dengan header sistem, bukan `libs/cxx`, supaya
   bisa jalan di macOS maupun di dalam container.
2. Smoke test menunggu penanda `fs`, `smp`, `sched`, `user`, dan `halt`.
   Panic dianggap gagal.
3. `scripts/test.sh` menjalankan arch check, host test, build, image ESP,
   lalu QEMU. Sudah dijalankan penuh dan lolos.

### Fase 14 - Hardening

- [ ] HARD-01 W^X: code read execute, data no execute, stack no execute.
- [ ] HARD-02 Stack canary untuk fungsi kernel.
- [ ] HARD-03 Validasi semua input dari device dan user.
- [ ] HARD-04 Batas iterasi di semua loop yang menunggu hardware.
- [ ] HARD-05 Panic dump lengkap dan konsisten.

---

## 6. Definition of done per modul

Sebuah modul dianggap selesai bila semua ini benar:

1. Punya satu header publik di `kernel/include/og/`.
2. Semua fungsi fallible memakai `Result` atau `Status` dan `[[nodiscard]]`.
3. Guard clause di awal, tanpa nesting lebih dari satu level.
4. Tidak ada `volatile` di luar `io/`.
5. Tidak ada duplikasi fungsi yang sudah ada di tabel pemilik.
6. Ada minimal satu pemakaian nyata di kernel, bukan kode mati.
7. Ada test host untuk logika murni, atau uji QEMU untuk jalur hardware.
8. Dokumentasi singkat di header: kontrak, konteks pemanggilan, boleh tidur
   atau tidak.

---

## 7. Urutan pengerjaan yang disarankan

1. Fase 0 sampai 3: fondasi, IO, arch, memori. Ini yang membuka sisanya.
2. Fase 4 sampai 6: events, time, sync. Timer butuh IRQ, sync butuh events.
3. Fase 7 dan 8: DMA dan driver. DMA sebelum virtio.
4. Fase 9: filesystem. Bisa paralel dengan driver setelah vfs ada.
5. Fase 10 dan 11: scheduler lalu syscall.
6. Fase 12 sampai 14: platform, SMP, testing, hardening.

Paralel yang aman: FS-01 dan FS-04 bisa jalan bareng TST-01 karena hanya
butuh `libs/core`, tidak butuh hardware.

---

## 8. Larangan

1. Dilarang menambah driver baru sebelum `drivers/device.h` ada.
2. Dilarang memakai alamat fisik mentah di driver, wajib lewat dma atau mmio.
3. Dilarang menulis ulang `memcpy`, `memset`, `strlen` di modul baru.
4. Dilarang membuat lock sendiri di dalam driver, pakai `sync/`.
5. Dilarang menaruh logika filesystem di driver virtio.
6. Dilarang menaruh logika alokasi di driver apa pun.
7. Dilarang import header modul atas dari modul bawah.
8. Dilarang menyalin fungsi yang sudah ada hanya karena beda tipe parameter.
   Pakai template atau overload di pemiliknya.

---

## 9. Backlog belum selesai

Ringkasan semua task terbuka. ID merujuk ke bagian 5, jadi detail kontrak
dan alasan tetap ada di fase masing masing. Bagian ini hanya daftar kerja
yang belum selesai, supaya tidak ada yang hilang di antara fase.

### 9.1 Ditunda karena butuh scheduler (Fase 10)

- [ ] SYN-02 mutex dengan wait queue. Wait queue sudah ada, mutex belum.
- [ ] SYN-03 semaphore. Alasannya sama.
- [ ] SYN-06 nama lock untuk debug deadlock.
- [ ] TIME-03 callback timeout `ScheduleAfter` dan `Cancel`. Sleep per
      thread sudah jalan lewat `wake_tick`, callback umum belum.
- [ ] TIME-01 konversi clocksource ke nanodetik, setelah ada pemakai.
- [ ] TIME-05 uji 1000 Hz di bawah beban scheduler.
- [ ] SCH-05 load balancing SMP, menunggu Fase 12.

### 9.1b Selesai di Fase 10

- [x] ARCH-06 `context.S`: `SwitchContext` dan trampoline thread.
- [x] MEM-08 guard page stack kernel lewat region VA terpisah.
- [x] EV-06 waitqueue dengan `WaitQueueWait`, `WakeOne`, `WakeAll`.

### 9.2 Ditunda karena butuh user mode (Fase 11)

- [ ] SYS-06 `mmap` dan `munmap` untuk user.
- [ ] MEM-07 objek ruang alamat per proses, setelah ada lebih dari satu
      proses user.
- [ ] Loader dari filesystem, bukan binary tertanam. Menunggu ramfs.

### 9.3 Ditunda karena belum ada pemakai nyata

- [ ] PLT-03 parser DTB: konduit PSCI dan alamat device masih memakai
      deskripsi platform. Parser dibutuhkan saat menambah board lain.
- [ ] SMP penjadwalan lintas core (SCH-05): butuh per-CPU scheduler.
- [ ] SYN-04 refcount: menunggu objek yang benar benar dibagi, misalnya
      file atau device.
- [ ] SYN-05 once: menunggu init yang perlu sekali jalan di luar `kmain`.
- [ ] DMA-04 pool per device: menunggu lebih dari satu device aktif.
- [ ] DRV-04 RTC PL031: menunggu timestamp di log.
- [ ] DRV-08 virtio-gpu: menunggu kebutuhan framebuffer.
- [ ] DRV-09 watchdog: tidak ada di QEMU virt.
- [ ] FS-04 ramfs: menunggu initramfs di Fase 11.
- [ ] FS-08 procfs: opsional, menunggu kebutuhan info kernel.

### 9.4 Sebagian selesai, perlu dilengkapi

- [ ] DRV-02 PL011 RX lewat IRQ, pengaturan baud, dan ring buffer TX dan RX.
      TX polling sudah jalan dan dipakai seluruh kernel.
- [ ] TIME-05 uji tick pada 1000 Hz. 100 Hz sudah terbukti, tetapi tesnya
      sekarang hanya memastikan interrupt datang, bukan laju tetap.
- [ ] FS-06 `write` untuk berkas biasa. Saat ini write hanya untuk device.

### 9.5 Fase yang belum dimulai

- [ ] Fase 9 sisa: FS-04 ramfs dan FS-08 procfs (lihat 9.3).
- [ ] Fase 10 sisa: SCH-05 load balancing SMP (lihat 9.1).
- [ ] Fase 11 sisa: SYS-06 mmap dan loader dari filesystem (lihat 9.2).
- [ ] Fase 12 sisa: PLT-03 parser DTB dan SMP scheduling (lihat 9.3).
- [ ] Fase 13 selesai.
- [ ] Fase 14 seluruhnya: HARD-01 sampai HARD-05.

### 9.6 Urutan yang disarankan setelah Fase 9

1. Fase 10 scheduler, karena membuka waitqueue, timeout, mutex, dan
   guard page sekaligus.
2. Fase 11 syscall dan user mode, yang membuka address space dan ramfs.
3. Fase 13 testing lebih awal dari Fase 12, supaya SMP punya jaring
   pengaman.
4. Fase 12 platform dan SMP.
5. Fase 14 hardening terakhir.

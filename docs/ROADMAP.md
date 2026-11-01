# Roadmap opengema-kernel

Dokumen ini jadi acuan tunggal untuk urutan kerja, batas modul, dan aturan
anti redundansi. Setiap task punya ID supaya bisa dirujuk di commit.

Status sekarang: Fase 0 sampai 14 selesai. Kernel sudah boot di QEMU virt,
punya MMU sendiri, allocator, interrupt, timer 1000 Hz, sinkronisasi lengkap,
virtio-blk, FAT32, scheduler round robin, user mode EL0, dua core aktif,
test otomatis, dan W^X.

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
  include/og/     header publik, satu direktori per subsistem
    core/         status, log, panic, assert, bootinfo, kmain
    arch/         barriers, cache, context, cpu, descriptor, mmu,
                  registers, spinlock, tlb, trap, user
    io/           mmio, console, print, register, framebuffer
    mem/          pmm, vmm, heap, mem_selftest
    events/       irq, trap, workqueue, event_selftest
    time/         timer, delay
    dma/          dma, dma_selftest
    drivers/      device, gic, rtc, uart, pci, virtio, virtio_blk
    fs/           vfs, block_device, fat32, devfs, ramfs, procfs, partition
    sched/        sched
    sync/         sync
    syscall/      syscall
    platform/     platform, fdt, psci, smp, memory_map
  arch/aarch64/   barriers.cpp, cache.S, context.S, vectors.S, mmu_primitives.cpp
  io/             mmio.cpp, console.cpp, print.cpp, framebuffer.cpp
  core/           log.cpp, panic.cpp, assert.cpp, init.cpp, version.cpp
  mem/            pmm.cpp, vmm.cpp, heap.cpp
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
  platform/       qemu_virt.cpp, fdt.cpp, psci.cpp
  boot/uefi/      efi_main.cpp, efi_console.cpp, efi_memmap.cpp, efi_exit.cpp
pubs/
  gemabox/        main.c, link.ld, README.md
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
- [x] ARCH-06 `context.S`: `SwitchContext` dan trampoline thread. Dipakai
      scheduler sejak Fase 10.
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
- [x] MEM-07 `AddressSpace`: level 0 table per proses, entri kernel tetap
      dibagi, ruang user di 1 TiB, ASID dipasang saat context switch,
      salinan user lewat page table, plus `AddressSpaceDestroy`.
- [x] MEM-08 Guard page stack kernel lewat region VA terpisah dengan
      halaman pertama tidak termap.
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
4. User space ditempatkan di 1 TiB, pada slot level 0 yang tidak dipakai
   kernel. Setiap `AddressSpace` menyalin root level 0, berbagi semua entri
   kernel, lalu memiliki tabel user sendiri. Kernel stack yang dibuat
   belakangan tetap terlihat karena entri level 0 kernel dipakai bersama.
5. Context switch memasang TTBR0 dengan ASID unik per ruang alamat. Salinan
   user berjalan lewat translasi page table, jadi tidak bergantung pada
   ruang mana yang sedang aktif.
6. Uji QEMU memetakan alamat user yang sama di dua ruang alamat dengan isi
   berbeda, menjalankan satu thread pada masing masing, dan memastikan
   keduanya membaca nilai yang benar.

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
- [x] EV-06 `waitqueue`: `WaitQueueWait`, `WakeOne`, `WakeAll`, plus
      enqueue dan wake untuk sleeping lock.
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
- [x] TIME-01 Clocksource: `TimeNowNs` dan `TimeNowMs`, dihitung dari
      `CNTPCT` dan `CNTFRQ`.
- [x] TIME-03 `ScheduleAfter` dan `CancelTimeout` dengan 32 slot, batch 8
      callback per tick, dan callback yang berjalan di IRQ context.
- [x] TIME-04 `og/delay.h`: `DelayUs` dan `DelayMs` berbasis counter,
      dipakai GIC untuk menunggu redistributor bangun.
- [x] TIME-05 Uji tick pada 1000 Hz. Gate hanya menuntut interrupt datang,
      karena laju tetap tidak bisa dijamin di bawah beban host.

**Selesai bila:** sleep dan timeout jalan tanpa busy loop.

Catatan hasil Fase 5: kriteria selesai fase ini bergantung pada scheduler,
jadi statusnya sebagian. Tick periodik dan delay sudah terbukti, sleep
belum bisa ada sebelum ada thread.

### Fase 6 - Sync

- [x] SYN-01 `og/arch/spinlock.h`: `Spinlock`, `IrqSpinlock`, `LockGuard`.
      Dipakai tabel IRQ dan workqueue, menggantikan `IrqSave` manual yang
      tersebar.
- [x] SYN-02 mutex dengan wait queue. Diuji dua thread yang menaikkan
      counter 50 kali masing masing, hasilnya tepat 100.
- [x] SYN-03 semaphore dengan wait queue. Diuji worker yang menunggu
      sampai `signal` dipanggil.
- [x] SYN-04 refcount dengan operasi atomik. Diuji sampai mencapai nol.
- [x] SYN-05 once dengan status idle, running, done. Fungsi hanya jalan
      sekali walau dipanggil dua kali.
- [x] SYN-06 nama lock lewat `set_name` dan `name`, dipakai self test.

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
- [x] DMA-04 `dma::Pool`: blok ukuran tetap, bitmap pemakaian, deteksi
      double free, dan `release`. Diuji di self test DMA.
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
- [x] DRV-02 PL011: TX polling, RX lewat IRQ 33, ring buffer 256 byte, dan
      baud 115200 dari clock 24 MHz. Diuji dengan injeksi serial.
- [x] DRV-03 GICv3: distributor, redistributor, CPU interface, enable,
      disable, acknowledge, end. Single core.
- [x] DRV-04 RTC PL031 di `0x09010000`. Waktu Unix dicetak saat boot dan
      didaftarkan ke registry device.
- [x] DRV-05 PCIe ECAM: scan bus, device, function, baca BAR, dan
      `EnableDevice` untuk menyalakan memory decoding.
- [x] DRV-06 virtio PCI transport: capability modern, feature negotiation,
      queue setup, notify.
- [x] DRV-07 virtio-blk: baca blok, polled completion, tanpa IRQ.
- [x] DRV-08 virtio-gpu 2D: control queue polled, resource 2D, attach
      backing, transfer, flush, dan scanout. Framebuffer console 32 bit
      dengan font 8x8 dan damage tracking ada di `og/io/framebuffer.h`.
- [x] DRV-09 watchdog: QEMU virt tidak punya device ini, jadi sengaja
      dilewati.

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
5. virtio-gpu memakai format B8G8R8A8 dan satu scanout. Console serial dan
   framebuffer digabung lewat `io::Tee`, jadi log tetap muncul di kedua
   tempat. Smoke test mengambil `screendump` lewat monitor dan memeriksa
   jumlah pixel teks.
6. BAR virtio-gpu ada di `0x8000000000`, di atas 4 GiB. Core kedua sempat
   fault karena TCR warisan firmware hanya mengizinkan alamat fisik 32 bit.
   `ActivateAddressSpace` sekarang menurunkan IPS dari PARange, sehingga
   semua core memakai ukuran alamat fisik yang sama.

### Fase 9 - Filesystem

- [x] FS-01 `og/vfs.h`: `Node`, `NodeOps`, `DirEntry`, `Stat`, `Mount`,
      dan tabel descriptor.
- [x] FS-02 `kernel/fs/vfs.cpp`: resolusi path, mount table dengan
      pencocokan prefix terpanjang. `..` ditolak, bukan ditebak.
- [x] FS-03 `kernel/fs/devfs.cpp`: `/dev/console`, `/dev/null`, `/dev/zero`.
- [x] FS-04 `kernel/fs/ramfs.cpp`: file di memori dengan write, grow, dan
      batas kapasitas.
- [x] FS-05 `kernel/fs/fat32.cpp`: baca saja, BPB, FAT chain, LFN,
      pemetaan nama 8.3.
- [x] FS-06 `kernel/fs/vfs.cpp`: `FsOpen`, `FsRead`, `FsWrite`, `FsSeek`,
      `FsClose`, `FsStat`. Write berlaku untuk device dan file ramfs.
- [x] FS-07 ESP dipasang sebagai root read-only saat boot.
- [x] FS-08 `procfs`: `/proc/uptime` dan `/proc/threads`.

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
- [x] SCH-05 Scheduler per-CPU: setiap core punya ready queue dan idle
      thread sendiri. Thread baru ditempatkan ke core dengan beban paling
      ringan. Thread tidak berpindah core setelah dibuat, sehingga context
      yang sedang berjalan tidak pernah diambil core lain.
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
5. SCH-05 memakai ready queue per-CPU. Thread baru ditempatkan ke core
   dengan beban paling ringan, lalu tetap di core itu. Invariant pentingnya:
   thread yang sedang berjalan tidak pernah ada di queue, sehingga core lain
   tidak mungkin menjalankan context yang sama.
6. Sleep dihitung dengan `TimeNowNs`, bukan jumlah tick global. Dengan dua
   core yang sama sama memberi tick, hitungan tick global akan membuat
   durasi tidur menjadi setengah.
7. `IrqSpinlock` menyimpan DAIF per-CPU. Versi lama menyimpannya di objek
   lock, sehingga core yang menunggu bisa menimpa state core pemilik lock.

### Fase 11 - Syscall dan user mode

- [x] SYS-01 `og/arch/user.h`: `EnterUserMode` menyiapkan `ELR`, `SP_EL0`,
      `SPSR`, lalu `eret` ke EL0.
- [x] SYS-02 ABI `svc #0`: nomor di `x8`, argumen di `x0..x5`, hasil di `x0`.
- [x] SYS-03 Syscall awal: `write`, `read`, `exit`, `yield`. `read` belum
      punya sumber input sehingga mengembalikan error.
- [x] SYS-04 `VmmCopyIn` dan `VmmCopyOut` dengan validasi region user dan
      translasi lewat page table ruang alamat pemanggil.
- [x] SYS-05 Loader program user dari filesystem: `GEMABOX.BIN` dibaca dari
      FAT, disalin ke ramfs, lalu dimuat ke halaman user.
- [x] SYS-06 `mmap` dan `munmap` untuk user. Region dialokasikan dari
      `mmap_next` milik address space; `munmap` membebaskan frame dan
      menghapus mapping.

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
4. Program user 217 byte ditulis dalam C freestanding (`pubs/gemabox/main.c`),
   dipanggil dengan `svc`, mencetak `hello from el0`, lalu `exit`.
5. Program user ditautkan di 1 TiB dan dijalankan di `AddressSpace` milik
   sendiri. Loader dan syscall memakai pointer ruang alamat itu, bukan
   variabel global.

### Fase 12 - Platform dan SMP

- [x] PLT-01 `og/platform.h`: deskripsi board dengan rentang UART, GIC,
      PCIe, RAM, jumlah CPU, dan konduit PSCI.
- [x] PLT-02 `kernel/platform/qemu_virt.cpp`: konfigurasi QEMU virt.
- [x] PLT-03 `og/fdt.h` dan `kernel/platform/fdt.cpp`: header FDT, cari
      node lewat path, baca properti, dan hitung anak. Dipakai untuk konduit
      PSCI dan jumlah CPU. Diuji dengan `acpi=off` karena firmware default
      memberi ACPI, bukan DTB.
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
3. Hook IRQ exit terpasang per-CPU. Setiap core punya timer, idle thread,
   dan ready queue sendiri setelah `SchedulerInitCpu`.
4. Secondary sekarang menjalankan scheduler, bukan hanya park di `wfi`.
   Uji boot memeriksa bahwa kedua core benar-benar menjalankan worker
   (`smp: worker cpus: 3`) dan IPI masih diterima.
5. GICv3 `IGROUPR` adalah register baca tulis. Menulis satu bit untuk
   mengaktifkan timer PPI menghapus bit group milik IPI, sehingga SGI
   masuk Group 0 dan tidak pernah dibaca lewat `ICC_IAR1_EL1`. Semua update
   group sekarang memakai read-modify-write.

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

- [x] HARD-01 W^X: rentang code diambil dari PE section table lewat
      `EFI_LOADED_IMAGE_PROTOCOL`, lalu blok page table dipecah ke 4 KiB.
      Code jadi read only dan executable, RAM lain no-execute, stack thread
      no-execute, halaman user tidak bisa dieksekusi dari EL1.
- [x] HARD-02 Stack canary dengan `-fstack-protector-strong`. Guard diisi
      dari generic counter saat boot. Simbol MSVC dan GNU disediakan.
- [x] HARD-03 Validasi input: usercopy memeriksa region, FAT memeriksa
      ukuran sektor dan batas cluster, virtio membatasi queue size, nomor
      syscall tak dikenal ditolak.
- [x] HARD-04 Batas iterasi: TX UART, wake GIC, poll virtio, rantai FAT, dan
      tunggu scheduler semuanya punya batas atau timeout.
- [x] HARD-05 Panic dump menambah `mpidr_el1`, `sp`, dan `current el`.

**Selesai bila:** semua di atas aktif tanpa merusak boot.

Catatan hasil Fase 14:

1. `VmmProtectRange` memecah blok 1 GiB menjadi 2 MiB, lalu 2 MiB menjadi
   4 KiB, sambil mempertahankan atribut parent.
2. Bug nyata: mapping Normal yang non-executable hanya menyetel `UXN`,
   sehingga EL1 masih boleh mengeksekusi heap. Verifikasi atribut halaman
   menangkapnya, dan sekarang `PXN` juga diset.
3. Verifikasi boot: `hardening: wx attributes ok` berarti text terbaca
   executable dan read only, sementara heap no-execute dan writable.

---

## 6. Definition of done per modul

Sebuah modul dianggap selesai bila semua ini benar:

1. Punya satu header publik di `kernel/include/og/<subsistem>/`.
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

### 9.1 Selesai setelah scheduler

- [x] SYN-02 mutex, SYN-03 semaphore, SYN-04 refcount, SYN-05 once, dan
      SYN-06 nama lock.
- [x] TIME-01 clocksource nanodetik dan TIME-03 callback timeout.
- [x] TIME-05 uji tick pada 1000 Hz.
- [x] SCH-05 scheduler per-CPU dan penempatan thread lintas core.

### 9.1b Selesai di Fase 10

- [x] ARCH-06 `context.S`: `SwitchContext` dan trampoline thread.
- [x] MEM-08 guard page stack kernel lewat region VA terpisah.
- [x] EV-06 waitqueue dengan `WaitQueueWait`, `WakeOne`, `WakeAll`.

### 9.2 Selesai setelah user mode

- [x] MEM-07 objek ruang alamat per proses, dipakai loader dan syscall.

### 9.3 Ditunda karena belum ada pemakai nyata

- [x] SMP penjadwalan lintas core (SCH-05): scheduler per-CPU sudah jalan.

### 9.4 Selesai

- [x] DRV-02 UART RX lewat IRQ dengan ring buffer dan baud.

### 9.5 Fase yang belum dimulai

- [x] Fase 9 selesai.
- [x] Fase 10 selesai.
- [x] Fase 11 selesai.
- [x] Fase 12 selesai.
- [x] Fase 13 selesai.
- [x] Fase 14 selesai.

### 9.6 Urutan yang disarankan setelah Fase 9

1. Fase 10 scheduler, karena membuka waitqueue, timeout, mutex, dan
   guard page sekaligus.
2. Fase 11 syscall dan user mode, yang membuka address space dan ramfs.
3. Fase 13 testing lebih awal dari Fase 12, supaya SMP punya jaring
   pengaman.
4. Fase 12 platform dan SMP.
5. Fase 14 hardening terakhir.

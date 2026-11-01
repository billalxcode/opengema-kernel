# opengema-kernel

Kernel ARM64 yang ditulis dengan C++20. Target awalnya QEMU virt dengan
boot UEFI.

Tujuannya belajar membangun kernel dari nol: mulai dari boot, komunikasi
serial, manajemen memori, sampai nanti interrupt dan scheduler.

## Cara pakai

Build di dalam podman, jalankan QEMU di host:

```sh
./scripts/get-firmware.sh   # siapkan firmware UEFI
./scripts/build.sh          # build kernel
./scripts/make-esp.sh       # siapkan disk boot
./scripts/run-qemu.sh       # jalankan, default tanpa GUI
```

Mau lihat jendela QEMU:

```sh
./scripts/run-qemu.sh --gui=on
```

## Test

Satu perintah untuk semua:

```sh
./scripts/test.sh
```

Isinya: cek arsitektur, host unit test, build kernel, siapkan image, lalu
smoke test QEMU. Smoke test gagal kalau penanda penting tidak muncul atau
ada panic.

Test lain:

```sh
./scripts/check-arch.sh   # aturan lapisan dan duplikasi
./scripts/test-host.sh    # unit test libs/core
./scripts/test-qemu.sh    # boot dan cek output
```

## Status

Sudah boot sampai kernel dengan MMU, memori, GIC, timer, virtio-blk,
virtio-gpu, FAT32, ramfs, procfs, scheduler, dan user mode. Program user pertama ada di
`pubs/gemabox`:

```
opengema-kernel: uefi entry
uefi: boot services exited
platform: dtb parsed
platform: cpus: 2
platform: psci conduit: 0
kernel: kmain entered
pmm: total frames: 131072
pmm: free frames: 122095
vmm: identity map active
hardening: wx attributes ok
mem test: pmm ok
mem test: vmm 4k ok
mem test: vmm 2m ok
mem test: heap ok
dma test: coherent ok
dma test: streaming ok
dma test: pool ok
pci: virtio device id: 4097
virtio-blk: queue size: 256
pci: virtio-gpu device id: 4176
virtio-gpu: display width: 1280
virtio-gpu: display height: 800
virtio-gpu: scanouts: 1
virtio-gpu: scanout ok
fs: mounted devfs and fat32
fs: ramfs gemabox bytes: 147
fs: ramfs write ok
fs: devfs console ok
fs: fat32 file read ok
GEMABOX.BIN
event test: irq and workqueue ok
gic: ready
psci: version: 65537
smp: cpus online: 2
smp: cpu online: 1
smp: ipi count: 1
timer: periodic tick ok
sched: round robin ok
sync: mutex, semaphore, refcount, once ok
time: timeout ok
uart: ready
uart: rx ok
user: program bytes: 147
hello from el0
mmap ok
user: exit ok
kernel: halt
```

Framebuffer console memakai virtio-gpu 1280x800 dan diuji otomatis dengan
`screendump` dari monitor QEMU.

Berikutnya: penjadwalan lintas core dan address space per proses.

Roadmap lengkap ada di `docs/ROADMAP.md`.

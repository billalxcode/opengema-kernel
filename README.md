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

## Status

Sudah boot sampai kernel dengan MMU, memori, GIC, timer, virtio, FAT32,
scheduler, dan user mode:

```
opengema-kernel: uefi entry
uefi: boot services exited
kernel: kmain entered
pmm: total frames: 131072
pmm: free frames: 122090
vmm: identity map active
mem test: pmm ok
mem test: vmm 4k ok
mem test: vmm 2m ok
mem test: heap ok
dma test: coherent ok
dma test: streaming ok
pci: virtio device id: 4097
virtio-blk: queue size: 256
virtio-blk: capacity sectors: 131072
virtio-blk: gpt header ok
fs: mounted devfs and fat32
fs: devfs console ok
fs: efi image bytes: 68608
fs: fat32 file read ok
EFI
STARTUP.NSH
event test: irq and workqueue ok
gic: ready
psci: version: 65537
smp: cpus online: 2
smp: cpu online: 1
smp: ipi count: 1
timer: frequency: 62500000
timer: ticks: 10
timer: periodic tick ok
sched: threads: 4
sched: worker a
sched: worker b
sched: workers done: 2
sched: round robin ok
user: program bytes: 51
hello from el0
user: exit ok
kernel: halt
```

Berikutnya: testing dan hardening.

Roadmap lengkap ada di `docs/ROADMAP.md`.

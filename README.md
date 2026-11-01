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

Sudah boot sampai kernel dengan MMU, exception handler, dan memori:

```
opengema-kernel: uefi entry
uefi: boot services exited
kernel: kmain entered
pmm: total frames: 131072
pmm: free frames: 122093
vmm: identity map active
mem test: pmm ok
mem test: vmm 4k ok
mem test: vmm 2m ok
mem test: heap ok
kernel: halt
```

Berikutnya: interrupt, timer, dan sinkronisasi.

Roadmap lengkap ada di `docs/ROADMAP.md`.

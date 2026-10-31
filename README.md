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

Sudah boot sampai kernel dan keluar dari boot services UEFI:

```
opengema-kernel: uefi entry
uefi: boot services exited
kernel: kmain entered
memory ranges: 29
uart base: 0x0000000009000000
kernel: halt
```

Berikutnya: MMU, interrupt, dan driver.

Roadmap lengkap ada di `docs/ROADMAP.md`.

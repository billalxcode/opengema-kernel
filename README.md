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
./scripts/run-qemu.sh       # jalankan
```

## Status

Sudah boot sampai kernel:

```
opengema-kernel: EFI boot
hello UART PL011
opengema-kernel: kmain boot
uart: PL011 0x09000000 ok
```

Berikutnya: MMU, ExitBootServices, dan exception handler.

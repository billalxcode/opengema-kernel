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

Di Linux dengan clang, lld, dan binutils AArch64 terpasang, semua script
bisa berjalan tanpa podman:

```sh
export OG_NATIVE=1
./scripts/get-firmware.sh
./scripts/build.sh
./scripts/make-esp.sh
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
./scripts/check-gsl.sh    # aplikasi hanya memakai libgemapi
./scripts/test-host.sh    # unit test libs/core
./scripts/test-tidy.sh    # clang-tidy dari compile_commands.json
./scripts/test-qemu.sh    # boot dan cek output
```

Host test dengan ASan dan UBSan:

```sh
./scripts/test-sanitize.sh
```

Coverage baris untuk host test inti, dengan ambang 55 persen:

```sh
./scripts/test-coverage.sh
```

Laporan lengkap ditulis ke `build/coverage/report.txt`.

## CI

GitHub Actions menjalankan build, host test, sanitizer, clang-tidy,
coverage, SDK probe, image boot, smoke test QEMU DTB, ACPI, dan PAN dengan
CPU `max`, serta tes persistensi. Workflow ada di `.github/workflows/ci.yml`
dan memakai LLVM 19 di runner Ubuntu 24.04.

Firmware UEFI dipin di `scripts/firmware.lock` ke paket Debian
`qemu-efi-aarch64` versi `2022.11-6+deb12u2`. `get-firmware.sh` memverifikasi
SHA256 paket, `AAVMF_CODE.fd`, dan `AAVMF_VARS.fd`, lalu mengunduh ulang bila
ada yang tidak cocok. Artefak `esp.img`, `BOOTAA64.EFI`, dan
`gema-system.img` diunggah dari workflow.

## Status

Sudah boot sampai kernel dengan MMU, memori, GIC, timer, virtio-blk,
virtio-gpu, FAT32, ramfs, procfs, scheduler, dan user mode. Program user
ada di `packages/gemabox`:

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
fs: gslfs nodes: 34
fs: gslfs mounted
fs: libgemapi bytes: 9352
fs: library probe ok
proc: uptime 5601 ms
fs: etc defaults copied
fs: tmpfs write ok
fs: writable mounts ok
fs: efi image bytes: 163328
fs: fat32 file read ok
fs: mounted gslfs, devfs, procfs, writable, fat32
fs: devfs console ok
event test: irq and workqueue ok
gic: ready
psci: version: 65537
smp: cpus online: 2
smp: cpu online: 1
smp: ipi count: 1
timer: periodic tick ok
sched: threads: 5
sched: workers done: 2
smp: worker cpus: 3
mem: address space switch ok
sched: round robin ok
sync: mutex, semaphore, refcount, once ok
time: timeout ok
user: program bytes: 9080
hello from el0
file ok
mmap ok
hello from el0
file ok
mmap ok
child ok
child ok
spawn ok
spawn ok
write ok
gemabox> help
commands: help, echo, cat, ls, clear, exit
gemabox> hello
hello from applications
gemabox> exit
user: exit pid: 1
user: exit status: 0
user: exit pid: 2
user: exit status: 0
user: exit pid: 3
user: exit status: 0
user: exit pid: 5
user: exit status: 0
user: frames before: 120015
user: frames after: 120015
process: remaining: 0
kernel: halt
```

Framebuffer console memakai virtio-gpu 1280x800 dan diuji otomatis dengan
`screendump` dari monitor QEMU. Program user ditulis dengan C++20 di atas
API publik `packages/libgemapi`, yang memakai lapisan ABI
`packages/libkgema` secara privat. Aplikasi tidak menyentuh `svc` atau
header kernel. Binary gemabox adalah ELF64 dynamic dan dibaca dari volume
system GSLFS: kernel memetakan `/bin/gemabox` dan
`/system/boot/ld-gem`, lalu `ld-gem` memuat `libgemapi.so.1` dan
`libkgema.so.1` dari `/system/libraries`. Kernel menjalankan
`/system/boot/init` sebagai PID 1; init menjalankan shell gemabox.
`/etc`, `/users`, `/var`, dan `/tmp` adalah mount writable pada path tetap.
Program berjalan di address space sendiri pada 1 TiB, dengan ASID yang
dipasang saat context switch. Dua proses gemabox berjalan bersamaan, masing
masing punya tabel file sendiri, lalu setiap proses menjalankan child dengan
argv dan menunggu. Proses writer menguji `chdir`, write ke FD, seek, dan
read di `/tmp`. Shell menerima input UART, menjalankan `help`, `echo`,
`cat`, `ls`, `clear`, `exit`, dan aplikasi eksternal `hello` dari
`/applications/hello/hello`. Jumlah frame bebas kembali ke angka sebelum
spawn.

Roadmap inti selesai. Berikutnya: pengembangan lanjutan di atas fondasi ini.

Roadmap lengkap ada di `docs/ROADMAP.md`. Rencana lanjutan dan analisis
kesenjangan ada di `docs/ROADMAP-NEXT.md`. Arsitektur layer, API, packaging,
dan layout filesystem ada di `docs/gsl/README.md`. Cara memakai SDK ada di
`docs/SDK.md`. Layout filesystem bersifat
mutlak dan bagian dari ABI; user tidak dapat mengubah mount point.

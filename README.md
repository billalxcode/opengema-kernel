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
./scripts/check-gsl.sh    # aplikasi hanya memakai libgemapi
./scripts/test-host.sh    # unit test libs/core
./scripts/test-qemu.sh    # boot dan cek output
```

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
fs: mounted devfs and fat32
fs: ramfs gemabox bytes: 9048
fs: ramfs ld-gem bytes: 10792
fs: ramfs libgemapi bytes: 9352
fs: ramfs libkgema bytes: 5064
fs: ramfs write ok
fs: devfs console ok
fs: fat32 file read ok
GEMABOX.ELF
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
user: program bytes: 2248
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
commands: help, echo, cat, exit
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
header kernel. Binary gemabox adalah ELF64 dynamic: kernel memetakan
executable dan `/ram/ld-gem`, lalu `ld-gem` memuat `libgemapi.so.1` dan
`libkgema.so.1` dari filesystem sebelum menjalankan `main`. Program berjalan
di address space sendiri pada 1 TiB, dengan ASID yang dipasang saat context
switch. Dua proses gemabox berjalan
bersamaan, masing masing punya tabel file sendiri, lalu setiap proses
menjalankan child dengan argv dan menunggu. Proses writer menguji `chdir`,
write ke FD, seek, dan read. Proses shell menerima input UART, menjalankan
`help`, dan keluar lewat `exit`. Jumlah frame bebas kembali ke angka
sebelum spawn.

Roadmap inti selesai. Berikutnya: pengembangan lanjutan di atas fondasi ini.

Roadmap lengkap ada di `docs/ROADMAP.md`. Rencana lanjutan dan analisis
kesenjangan ada di `docs/ROADMAP-NEXT.md`. Arsitektur layer, API, packaging,
dan layout filesystem ada di `docs/gsl/README.md`.

---
name: opengema-kernel-debug
description: Cara debug kernel ARM64 QEMU virt UEFI dengan GDB. Dipakai saat hang, fault, atau boot gagal.
---

# opengema-kernel-debug

## 1. Triage dengan guard clause

Cek dalam urutan ini, berhenti di gagal pertama:

1. Jika build gagal, jangan jalankan QEMU. Perbaiki `clang --target` dan `lld` dulu.
2. Jika QEMU langsung keluar, cek firmware `QEMU_EFI.fd` ada dan path benar.
3. Jika tidak ada output serial, cek `-serial mon:stdio`, alamat UART `0x09000000`, dan kabel `-M virt`.
4. Jika stuck setelah `ExitBootServices`, cek `GetMemoryMap` key dan map key kadaluarsa.
5. Jika data abort setelah MMU on, cek `MAIR`, `TCR`, `TTBR0`, atribut device vs normal.
6. Jika synchronous exception, decode `ESR_EL1`, `FAR_EL1`, `ELR_EL1` dulu sebelum tebak.

## 2. Perintah run standar (pflash + virtio ESP)

```sh
VARS_TMP="$(mktemp /tmp/AAVMF_VARS.XXXXXX.fd)"
cp firmware/AAVMF_VARS.fd "$VARS_TMP"
qemu-system-aarch64 \
  -M virt,gic-version=3 \
  -cpu cortex-a72 \
  -m 512M \
  -nic none \
  -drive if=pflash,format=raw,file=firmware/AAVMF_CODE.fd,readonly=on \
  -drive if=pflash,format=raw,file="$VARS_TMP" \
  -drive file=esp.img,format=raw,if=virtio \
  -serial mon:stdio \
  -display none \
  -s -S -d guest_errors,int,cpu_reset
```

- Firmware dari paket Debian `qemu-efi-aarch64` via `./scripts/get-firmware.sh`.
- `-nic none` agar EDK2 tidak nyangkut di PXE.
- ESP adalah GPT image (`./scripts/make-esp.sh`), bukan `fat:rw`.
- `-s` = gdb di `:1234`. `-S` = freeze di awal.
- `-d guest_errors,int` untuk trace interrupt dan exception.
- Jangan pakai `-bios` + `-kernel` untuk uji UEFI. Pakai pflash + ESP.

GDB:

```sh
aarch64-none-elf-gdb build/BOOTAA64.EFI
(gdb) target remote :1234
(gdb) add-symbol-file build/BOOTAA64.EFI 0x address_dari_loader
(gdb) b kmain
(gdb) c
(gdb) bt
(gdb) info registers
(gdb) x/8i $pc
(gdb) p/x $esr_el1
(gdb) p/x $far_el1
(gdb) p/x $elr_el1
```

Untuk bare metal setelah ExitBootServices, pakai `symbol-file build/kernel.elf` dengan VMA yang sama dengan link address.

## 3. Decode ESR_EL1 cepat

- EC ada di bits [31:26]. Contoh umum:
  - `0x21` = instr abort EL1
  - `0x24` = data abort EL1
  - `0x0F` = SVC
  - `0x3A` = SError
- ISS bit [24:0] untuk detail. DFSC di [5:0] untuk abort:
  - `0b000100` = translation fault level 0
  - `0b000101` = translation fault level 1
  - `0b000110` = translation fault level 2
  - `0b000111` = translation fault level 3
  - `0b001101` = permission fault
- Selalu cetak `ESR_EL1`, `FAR_EL1`, `ELR_EL1`, `SPSR_EL1`, `VBAR_EL1` saat panic.

Panic print minimal harus ada:

```
PANIC ESR=... FAR=... ELR=... SPSR=...
```

## 4. Teknik blackbox

- Jika tidak ada GDB, tambah `uart_puthex` di tiap tahap: `boot -> vbar -> mmu map -> mmu on -> jump`.
- Pakai `objdump -d` dan `readelf -h` untuk cek PE/COFF vs ELF dan entry point.
- Pakai `llvm-nm` untuk cek simbol `efi_main` dan `kmain` tidak ter-strip.
- Simpan QEMU log ke file saat serial noise: `-serial file:qemu.log`.
- Build di podman, run di host. Jangan debug di dalam podman kecuali perlu strace.

```sh
podman build -f Containerfile -t opengema-build .
podman run --rm -v ./:/work -w /work opengema-build cmake --build build
./scripts/run-qemu.sh
```

## 5. Larangan

- Jangan ubah dua hal sekaligus saat debug (MMU + UART). Bisect satu variabel.
- Jangan tutup dengan tebakan. Tulis alamat dan register yang diobservasi.
- Jangan pakai printf semihosting sebagai satu-satunya sumber. UART PL011 adalah sumber benar untuk virt.

## References

- Baca `references/triage.md` untuk urutan cek gagal dulu.
- Baca `references/qemu-virt.md` untuk flags run standar.
- Baca `references/gdb-esr.md` untuk GDB dan decode ESR.

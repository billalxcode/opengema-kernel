# Debugging opengema-kernel

Panduan singkat untuk memburu masalah boot, fault, dan hang.

## 1. Triage cepat

Berhenti di kegagalan pertama:

1. Build gagal: perbaiki toolchain sebelum menjalankan QEMU.
2. QEMU keluar cepat: cek path `firmware/AAVMF_CODE.fd` dan `esp.img`.
3. Serial kosong: cek `-serial`, base UART `0x09000000`, dan `-M virt`.
4. Stuck setelah ExitBootServices: cek map key dan urutan capture.
5. Fault setelah MMU: cek `MAIR`, `TCR`, `TTBR0`, atribut device.
6. Exception sinkron: baca `ESR`, `FAR`, `ELR` sebelum menebak.
7. Hang tanpa output: pasang penanda langsung ke console tanpa lock untuk
   melihat titik terakhir.

## 2. Jalankan dengan GDB

```sh
VARS_TMP="$(mktemp /tmp/AAVMF_VARS.XXXXXX)"
cp firmware/AAVMF_VARS.fd "$VARS_TMP"
qemu-system-aarch64 \
  -M virt,gic-version=3,acpi=off -cpu cortex-a72 -smp 2 -m 512M -nic none \
  -drive if=pflash,format=raw,file=firmware/AAVMF_CODE.fd,readonly=on \
  -drive if=pflash,format=raw,file="$VARS_TMP" \
  -drive file=esp.img,format=raw,if=virtio \
  -serial mon:stdio -display none -s -S
```

Di GDB:

```sh
(gdb) target remote :1234
(gdb) break efi_main
(gdb) continue
(gdb) info registers
(gdb) x/8i $pc
```

Untuk kode setelah MMU aktif, simbol `build/BOOTAA64.EFI` tetap berlaku
karena image dimuat identity.

## 3. Decode ESR

- EC ada di bit 31:26.
- `0x15` SVC64.
- `0x20` instruction abort dari EL0.
- `0x21` instruction abort dari EL1.
- `0x24` data abort dari EL0 (lower EL).
- `0x25` data abort dari EL1 (same EL).
- `0x07` akses FP/SIMD/SVE yang di-trap. Kernel memakai
  `-mgeneral-regs-only`, jadi ini tanda ada kode FP tak sengaja.
- DFSC bit 5:0 untuk abort: `0b000100` sampai `0b000111` translation fault
  level 0 sampai 3, `0b001101` permission fault level 3.

Panic sudah mencetak `esr_el1`, `esr_ec`, `esr_iss`, `far_el1`, `elr_el1`,
`spsr_el1`, dan `vbar_el1`.

## 4. Teknik tanpa GDB

- Tambah penanda `io::ConsoleWriteTextActive` di tahap yang dicurigai.
  Fungsi ini tidak memakai print lock, jadi tetap berguna saat lock macet.
- Simpan log serial ke file dengan `-serial file:qemu.log`.
- Aktifkan `-d int,guest_errors -D int.log` untuk melihat exception per
  CPU. Hati-hati, log bisa ratusan MB.
- Cek entry point dan simbol dengan `llvm-nm` dan `objdump -d`.
- Ambil isi framebuffer dengan monitor HMP: `screendump /tmp/screen.ppm`.
  Smoke test memakai ini untuk membuktikan console grafis benar-benar
  menggambar.
- Bandingkan perilaku HVF dan TCG dengan `-accel tcg` untuk memisahkan
  masalah virtualisasi dari kode kernel.

## 5. Masalah yang pernah terjadi

- `EFI_BOOT_SERVICES` diawali header 24 byte. Offset salah membuat semua
  service bergeser.
- Firmware mematikan memory decoding PCI. Semua register virtio terbaca
  `0xFFFF` sampai bit memori di command register dinyalakan.
- ECAM QEMU 11 ada di `0x4010000000`, bukan `0x3F000000`.
- Stub vector yang menimpa `x0` sebelum menyimpan register merusak argumen
  syscall.
- BAR virtio-gpu ada di `0x8000000000`. Core sekunder mewarisi TCR dengan
  IPS 0 (alamat fisik 32 bit), sehingga akses ke BAR di atas 4 GiB memicu
  translation fault level 3. IPS sekarang diturunkan dari PARange di
  `ActivateAddressSpace`.
- GICv3 `IGROUPR` bersifat baca tulis. Menulis satu bit untuk PPI baru
  menghapus group bit PPI lain. Akibatnya IPI pindah ke Group 0 dan tidak
  terlihat oleh `ICC_IAR1_EL1`. Selalu read-modify-write untuk register
  group.
- `IrqSpinlock` harus menyimpan DAIF per-CPU. Menyimpannya di objek lock
  membuat core yang menunggu menimpa state core pemilik.
- Address space per proses memakai slot level 0 di 1 TiB. Entri kernel
  dibagi, jadi stack kernel yang dibuat setelah proses tetap terlihat.
  TTBR0 memakai ASID, dan `TCR_EL1.A1` harus nol agar ASID diambil dari
  TTBR0.
- Fault yang terjadi saat print lock dipegang membuat panic menunggu lock
  yang sama. Saat memburu kasus seperti ini, simpan ESR, FAR, dan ELR ke
  variabel global di stub vektor sebelum panic, lalu baca lewat monitor.
- Compiler bisa memakai SIMD untuk menyalin struct kecil. Core sekunder
  dengan FPEN nol akan trap. Solusi permanen: `-mgeneral-regs-only`.

# Triage Boot

Urutan cek, berhenti di gagal pertama:

1. Build gagal: cek `clang --target` dan link `lld`.
2. QEMU keluar cepat: cek path `QEMU_EFI.fd`.
3. Serial kosong: cek `-serial mon:stdio` dan base `0x09000000`.
4. Gagal setelah `ExitBootServices`: cek `GetMemoryMap` key basi.
5. Abort setelah MMU on: cek `MAIR, TCR, TTBR0`, atribut device vs normal.
6. Exception sinkron: decode `ESR, FAR, ELR` dulu.

Teknik: `uart_puthex` tiap tahap. `objdump -d`, `readelf -h`, `llvm-nm` untuk entry dan simbol. Ubah satu variabel per iterasi.

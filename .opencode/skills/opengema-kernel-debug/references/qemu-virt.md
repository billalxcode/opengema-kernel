# QEMU virt

Machine: `-M virt,gic-version=3 -cpu cortex-a72 -m 512M`.
Firmware: `firmware/AAVMF_CODE.fd` + `AAVMF_VARS.fd` dari paket
Debian `qemu-efi-aarch64` (lihat `scripts/get-firmware.sh`).
Jangan pakai file `QEMU_EFI.fd` dari Linaro, URL lama sudah mati.

Pflash, bukan -bios:

```
-drive if=pflash,format=raw,file=firmware/AAVMF_CODE.fd,readonly=on
-drive if=pflash,format=raw,file=$VARS_TMP
```

Disk ESP: GPT image `esp.img` via `scripts/make-esp.sh`, attach
sebagai virtio. `fat:rw:` tidak reliable untuk boot EDK2.
Tambah `-nic none` agar tidak nyangkut PXE.

Build di podman, run QEMU di host macOS agar dapat HVF.

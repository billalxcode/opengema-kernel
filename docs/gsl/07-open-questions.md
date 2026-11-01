# 07 - Pertanyaan dan Opsi

Dokumen ini menyimpan brainstorming yang belum diputuskan. Setiap item
memiliki opsi, rekomendasi awal, dan status.

## 1. Static atau dynamic lebih dulu

Opsi:

- A. Static library dulu.
- B. Langsung dynamic library.

Keputusan: B. Dynamic library langsung. `ld-gem` tetap statis sebagai
bootstrap karena harus berjalan sebelum library dinamis tersedia.

Status: diputuskan.

## 2. Filesystem system volume

Opsi:

- A. FAT32.
- B. ext2 read-only.
- C. Filesystem package khusus GSL.

Keputusan: C. GSLFS read-only khusus. FAT32 tetap boleh untuk volume data
dan pertukaran file, bukan system volume.

Status: diputuskan.

## 3. Root filesystem

Opsi:

- A. Satu root Linux-like.
- B. Drive letter Windows-like.
- C. Hybrid dengan `/volumes/c`.

Keputusan: A. Satu root Unix-like lowercase, tanpa drive letter.

Status: diputuskan. Detail di `03-filesystem-layout.md`.

## 4. Busybox atau binary terpisah

Opsi:

- A. gemabox multi-call binary untuk command inti.
- B. Satu binary per command.
- C. Hybrid: applet inti di gemabox, aplikasi besar terpisah.

Keputusan: C. Hybrid: applet inti di gemabox, aplikasi besar terpisah.

Status: diputuskan.

## 5. Mount per library

Opsi:

- A. Setiap library di-mount sendiri.
- B. Satu volume system berisi semua library.
- C. Volume per package.

Keputusan: B. Satu volume system berisi semua library. Mount adalah operasi
filesystem; library di-load dari volume yang sudah dipasang.

Status: diputuskan.

## 6. Kernel atau user loader

Opsi:

- A. Kernel memuat executable dan dynamic library.
- B. Kernel memuat executable, user dynamic linker memuat library.
- C. Kernel hanya memuat flat binary, semua ELF di user.

Keputusan: B. Kernel memvalidasi dan memetakan executable serta membaca
`PT_INTERP`; `ld-gem` menyelesaikan dependency dan relokasi di user space.

Status: diputuskan.

## 7. Manifest

Opsi:

- A. `key=value` sederhana.
- B. JSON.
- C. Binary TLV.

Keputusan: A. Format `key=value` sederhana.

Status: diputuskan.

## 8. Init

Opsi:

- A. Kernel langsung menjalankan shell.
- B. Kernel menjalankan init, init menjalankan shell.
- C. Kernel menjalankan service manager.

Keputusan: B. Kernel menjalankan init, init menjalankan shell dan service.

Status: diputuskan.

## 9. Capability

Opsi:

- A. Path dan permission Unix.
- B. Capability handle.
- C. Hybrid path + capability.

Keputusan: C. Permission path untuk filesystem, capability handle untuk
device dan operasi sensitif.

Status: diputuskan.

## 10. Format path

Opsi:

- A. Case-sensitive Linux.
- B. Case-insensitive Windows.
- C. Case-sensitive di system, case-insensitive di data.

Keputusan: A. Case-sensitive mengikuti Unix.

Status: diputuskan.

## 11. Nama direktori

Keputusan: lowercase Unix.

```
/system
  boot/
  libraries/
  services/
  config/
/applications
/commands
/etc
/users
/tmp
/var
/dev
/proc
/volumes
```

Status: diputuskan. Detail ada di `03-filesystem-layout.md`.

## 12. API namespace

Opsi:

- `gemapi::`
- `gema::`
- `gsl::`

Rekomendasi: `gemapi::` agar langsung terlihat sebagai API publik.

Status: usulan.

## 13. Driver user

Opsi:

- A. Semua driver di kernel.
- B. Driver user untuk device tertentu.
- C. Driver user hanya setelah IOMMU dan capability.

Rekomendasi: A untuk sekarang, C untuk jangka panjang.

Status: ditunda.

## 14. GUI

Opsi:

- A. Shell teks dulu.
- B. GUI langsung.
- C. Shell teks dan API grafis, GUI menyusul.

Rekomendasi: C.

Status: ditunda.

## 15. Recovery

Opsi:

- A. Panic kernel.
- B. Recovery init.
- C. Dua system volume dengan rollback.

Rekomendasi: B dulu, C nanti.

Status: terbuka.

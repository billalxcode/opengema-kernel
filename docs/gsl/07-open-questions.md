# 07 - Pertanyaan dan Opsi

Dokumen ini menyimpan brainstorming yang belum diputuskan. Setiap item
memiliki opsi, rekomendasi awal, dan status.

## 1. Static atau dynamic lebih dulu

Opsi:

- A. Static library dulu.
- B. Langsung dynamic library.

Rekomendasi: A. Static dulu untuk membuktikan boundary aplikasi dan API.
Setelah ELF loader dan system volume stabil, pindah ke dynamic.

Status: usulan.

## 2. Filesystem system volume

Opsi:

- A. FAT32.
- B. ext2 read-only.
- C. Filesystem package khusus GSL.

Rekomendasi: B atau C untuk system volume jangka panjang. FAT32 dipakai
untuk data dan pertukaran file. FAT32 tidak punya permission dan symlink.

Status: terbuka.

## 3. Root filesystem

Opsi:

- A. Satu root Linux-like.
- B. Drive letter Windows-like.
- C. Hybrid dengan `/Volumes/C`.

Rekomendasi: A, dengan C sebagai adaptasi jika diperlukan.

Status: usulan kuat.

## 4. Busybox atau binary terpisah

Opsi:

- A. gemabox multi-call binary untuk command inti.
- B. Satu binary per command.
- C. Hybrid: applet inti di gemabox, aplikasi besar terpisah.

Rekomendasi: C.

Status: usulan.

## 5. Mount per library

Opsi:

- A. Setiap library di-mount sendiri.
- B. Satu volume system berisi semua library.
- C. Volume per package.

Rekomendasi: B. Mount adalah operasi filesystem. Library di-load dari
volume yang sudah dipasang. Ini lebih sederhana dan tidak membuat mount
table penuh.

Status: usulan kuat.

## 6. Kernel atau user loader

Opsi:

- A. Kernel memuat executable dan dynamic library.
- B. Kernel memuat executable, user dynamic linker memuat library.
- C. Kernel hanya memuat flat binary, semua ELF di user.

Rekomendasi: B. Kernel memvalidasi dan memetakan executable; dynamic linker
menyelesaikan dependency dan relokasi.

Status: usulan.

## 7. Manifest

Opsi:

- A. `key=value` sederhana.
- B. JSON.
- C. Binary TLV.

Rekomendasi: A untuk awal, B jika parser JSON sudah ada.

Status: usulan.

## 8. Init

Opsi:

- A. Kernel langsung menjalankan shell.
- B. Kernel menjalankan init, init menjalankan shell.
- C. Kernel menjalankan service manager.

Rekomendasi: B. Init kecil, shell dan service dikelola user space.

Status: usulan kuat.

## 9. Capability

Opsi:

- A. Path dan permission Unix.
- B. Capability handle.
- C. Hybrid path + capability.

Rekomendasi: C. Capability untuk device dan operasi sensitif, permission
path untuk file.

Status: terbuka.

## 10. Format path

Opsi:

- A. Case-sensitive Linux.
- B. Case-insensitive Windows.
- C. Case-sensitive di system, case-insensitive di data.

Rekomendasi: A untuk kesederhanaan VFS.

Status: usulan.

## 11. Nama direktori

Opsi:

- `/Applications` atau `/Apps`
- `/System/Libraries` atau `/Lib`
- `/Devices` atau `/Dev`

Rekomendasi: nama panjang yang jelas untuk GSL, dengan alias pendek nanti
jika perlu.

Status: usulan.

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

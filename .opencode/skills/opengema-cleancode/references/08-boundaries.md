# Bab 8 - Boundaries

Sumber: Clean Code, bab 8.

## Inti

Boundary adalah batas antara kode kita dan kode pihak ketiga, hardware,
ABI, atau sistem lain. Boundary yang bersih mengisolasi detail eksternal
dan melindungi kode inti dari perubahan di luar kendali kita.

Prinsip utama:

- Gunakan third-party code dengan pembungkus yang jelas.
- Pelajari boundary dengan learning test.
- Jangan biarkan tipe eksternal menyebar ke seluruh sistem.
- Gunakan adapter untuk menerjemahkan interface.
- Kode yang belum ada bisa diuji dengan fake atau stub.
- Boundary harus punya kontrak dan test.

## Aturan

- Bungkus API eksternal di satu modul.
- Jangan memanggil register hardware langsung dari logika bisnis.
- Jangan mengekspos tipe library ke layer atas.
- Buat adapter kecil dan satu arah.
- Uji adapter terhadap perilaku yang kita butuhkan, bukan seluruh API.
- Jika API eksternal berubah, hanya adapter yang berubah.

## Penerapan di OpenGEMA

- `BlockReadAdapter` menjembatani virtio-blk ke `BlockDevice`.
- `libkgema` adalah boundary syscall; `libgemapi` adalah boundary API.
- `libgemapi` tidak boleh membocorkan tipe `kgema`.
- UEFI, PCI, virtio, GIC, dan UART adalah boundary hardware.
- GSLFS adalah boundary format on-disk; jangan expose struct mentah.
- `ld-gem` adalah boundary dynamic linking.
- Learning test: host ELF probe, GSLFS verifier, QEMU smoke test.
- Jangan memakai tipe UEFI di kode filesystem atau proses.

## Pertanyaan review

- Apakah kode inti bergantung pada tipe eksternal?
- Apakah ada adapter yang bisa diuji terpisah?
- Apakah boundary punya test untuk perilaku yang dipakai?
- Apakah perubahan API eksternal hanya berdampak di adapter?
- Apakah detail hardware bocor ke aplikasi?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 8.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 8:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter08.xhtml
- Catatan bab 8:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter8
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29

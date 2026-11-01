# Bab 2 - Meaningful Names

Sumber: Clean Code, bab 2.

## Inti

Nama adalah alat utama abstraksi. Nama yang baik mengungkap maksud, alasan
keberadaan, dan cara pemakaian tanpa perlu komentar. Nama yang buruk
memaksa pembaca membangun peta mental dan menyembunyikan bug.

Prinsip utama:

- Intention-revealing: nama menjelaskan maksud.
- Hindari disinformasi dan nama yang menyesatkan.
- Buat distingsi yang bermakna, bukan angka atau tipe.
- Gunakan nama yang bisa diucapkan dan dicari.
- Hindari encoding tipe, prefix, dan singkatan internal.
- Hindari mental mapping.
- Class memakai noun; method memakai verb.
- Jangan lucu, jangan pun, jangan singkatan pribadi.
- Satu kata untuk satu konsep.
- Gunakan nama domain solusi atau domain masalah.
- Tambahkan konteks yang bermakna, bukan konteks berlebihan.

## Aturan

- Nama harus cukup panjang sesuai scope. Variabel loop boleh pendek.
- Nama konstanta harus menjelaskan nilai dan tujuan.
- Nama fungsi harus menjelaskan efek samping jika ada.
- Nama tipe harus noun atau noun phrase.
- Hindari `data`, `info`, `manager`, `processor`, `temp`, `foo`, `bar`.
- Jangan memakai tipe di nama seperti `strName` atau `u64Count`.
- Jangan memakai singkatan yang tidak dikenal di domain.

## Penerapan di OpenGEMA

Style proyek:

- Type: `PascalCase`, misalnya `AddressSpace`, `GslfsNode`.
- Free function: `PascalCase`, misalnya `VmmMapUser`, `FsMount`.
- Method: `snake_case`, misalnya `file.read`, `node.lookup`.
- Variabel lokal: `snake_case`.
- Konstanta: `kPascalCase`, misalnya `kMaxNodes`.
- Namespace: `og::fs`, `gemapi::fs`.

Contoh perbaikan:

- `buf`, `tmp`, `len` hanya boleh untuk scope sangat pendek.
- `gslfs_node` untuk struct, `gslfs_node_count` untuk jumlah.
- `DoList` menjelaskan syscall handler; `ListDirectory` untuk helper.
- `SetupWritableFs` menjelaskan tujuan; bukan `Init2`.
- `kHelloPath` adalah path test; bukan `p1`.

## Pertanyaan review

- Apakah nama menjelaskan maksud tanpa komentar?
- Apakah ada dua konsep berbeda dengan nama yang mirip?
- Apakah ada nama yang menyesatkan tipe atau isi?
- Apakah nama konsisten dengan modul lain?
- Apakah nama domain dipakai dengan benar?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 2.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 2:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter02.xhtml
- Catatan bab 2:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter2
- Ringkasan bab 2:
  https://medium.com/geekculture/clean-code-chapter-2-meaningful-names-summary-1255cdbf2df3
- Analisis bab 2:
  https://aradaelli.com/blog/clean-code-chapter-2/
- Ringkasan bab 1 dan 2:
  https://vuyisile.com/clean-code-chapters-1-2-summary/

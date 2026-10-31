---
name: opengema-exploration
description: Cara crawling dan eksplorasi docs untuk kernel. Dipakai saat butuh referensi ARM, UEFI, QEMU, atau C++.
---

# opengema-exploration

## 1. Kapan pakai apa

Terapkan guard clause untuk pilih tool:

- Jika belum punya URL, jangan scrape. Search dulu.
- Jika sudah punya URL spesifik, jangan search lagi. Fetch langsung.
- Jika butuh docs library atau API, pakai Context7 dulu sebelum web umum.
- Jika butuh banyak halaman satu domain, pakai map lalu crawl. Jangan scrape satu per satu manual.
- Jika butuh bukti kode nyata, pakai developer search (GitHub issues, PR, README).

## 2. Alur firecrawl

Simpan semua hasil di `.firecrawl/` agar tidak penuhi konteks. Baca dengan `jq`.

```sh
# 1. search dulu, jangan langsung scrape
firecrawl search "QEMU virt aarch64 memory map PL011 GIC" -o .firecrawl/search-virt.json --json
jq -r '.data.web[].url' .firecrawl/search-virt.json

# 2. scrape URL terpilih saja
firecrawl scrape https://example.com/docs -o .firecrawl/scrape-docs.json --json

# 3. jika butuh struktur situs
firecrawl map https://example.com/docs -o .firecrawl/map.json --json

# 4. jika butuh banyak halaman
firecrawl crawl https://example.com/docs --max-pages 20 -o .firecrawl/crawl.json --json
```

Untuk kode:

```sh
firecrawl search "PL011 UART init QEMU virt" --categories developer -o .firecrawl/dev-uart.json --json
jq -r '.data.developer[] | .url, .description' .firecrawl/dev-uart.json
```

Kirim feedback setelah search dipakai agar hemat kredit. Jika Env `FIRECRAWL_NO_SEARCH_FEEDBACK=1` diset, hormati dan jangan kirim.

## 3. Alur Context7 untuk API

1. `resolve-library-id` dengan nama resmi, contoh `Next.js` bukan `nextjs`.
2. Pilih ID format `/org/project`, lihat reputasi dan snippet count.
3. `query-docs` satu konsep per call. Jangan gabung routing dan auth dan cache dalam satu query.
4. Jawab berdasar hasil fetch, bukan ingatan.

Untuk kernel, Context7 cocok untuk CMake, Clang, EDK2 docs. Untuk ARM ARM dan UEFI spec, lebih cocok fetch langsung dari developer.arm.com dan uefi.org karena itu sumber primer.

## 4. Verifikasi sumber

- Prioritas: spec primer (ARM ARM, UEFI spec, QEMU docs) di atas blog.
- Jika blog dan spec beda, menangkan spec. Tulis konfliknya.
- Jangan buat URL tebakan. Hanya sajikan URL yang sudah di fetch sukses.
- Jika fetch 403 seperti wiki.osdev.org kadang block bot, catat sebagai gagal fetch dan pakai mirror atau search snippet + verifikasi silang. Jangan klaim sudah baca penuh.
- Simpan kutipan penting dengan URL dan tanggal akses di catatan.

## 5. Pola eksplorasi codebase

- Mulai dengan `read` direktori, lalu `glob`, lalu `grep` pola seperti `VBAR|TTBR|ESR_EL1|0x09000000`.
- Gunakan subagent `explore` untuk codebase besar agar hemat konteks.
- Baca file kecil secara utuh. Jangan potong di tengah struct bitfield.
- Akhiri dengan ringkasan: apa yang ditemukan, apa yang belum, file apa yang jadi sumber.

## References

- Baca `references/firecrawl-workflow.md` untuk alur search dan scrape.
- Baca `references/source-priority.md` untuk prioritas spec primer.

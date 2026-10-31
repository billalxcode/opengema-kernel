---
name: opengema-antislop
description: Larangan em dash dan unicode non keyboard untuk semua tulisan.
---

# opengema-antislop

Aturan ini berlaku untuk semua hal mengenai penulisan: chat, kode, komentar, docs, commit message, dan output tool.

## 1. DILARANG KERAS em dash

- Jangan pakai karakter em dash U+2014 dalam bentuk apapun.
- Jika perlu tanda pisah, pakai hyphen biasa - (U+002D).
- Contoh benar: `UART init - cek base address dulu`.
- Contoh salah: memakai em dash sebagai pemisah kalimat.

## 2. DILARANG KERAS unicode yang tidak ada di keyboard

- Hanya boleh pakai karakter yang ada di keyboard standar US: ASCII 32 sampai 126 plus newline dan tab.
- Artinya: huruf A-Z a-z, angka 0-9, dan simbol ` ~ ! @ # $ % ^ & * ( ) _ - + = { } [ ] | \ : ; " ' < > , . ? /` plus spasi dan newline.
- Dilarang: en dash U+2013, em dash U+2014, ellipsis U+2026, bullet U+2022, arrow, star, check, quotes lengkung, dan semua huruf non ASCII.
- Jika perlu tiga titik, tulis `...` tiga kali (U+002E). Jika perlu strip, tulis `-`.
- Komentar kode dan string user facing wajib ASCII juga, kecuali user eksplisit minta unicode.

## 3. Cara cek sebelum kirim

- Cek manual: tidak ada karakter aneh di output.
- Cek via shell untuk file:

```sh
if grep -R --include="*.md" --include="*.cpp" --include="*.h" -P "[^\x00-\x7F]" .; then echo "FAIL unicode non ASCII found"; else echo "PASS ASCII only"; fi
```

- Jika FAIL, ganti dengan padanan ASCII keyboard lalu cek ulang.
- Output ditolak jika masih ada em dash atau unicode non keyboard.

## References

- Baca `references/no-em-dash.md` untuk aturan em dash.
- Baca `references/no-unicode.md` untuk aturan unicode keyboard.

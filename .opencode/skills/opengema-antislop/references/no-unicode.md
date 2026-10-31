# No unicode non keyboard

Hanya ASCII 32-126 plus newline dan tab yang boleh.

Boleh:
- A-Z a-z 0-9
- spasi dan newline
- ` ~ ! @ # $ % ^ & * ( ) _ - + = { } [ ] | \ : ; " ' < > , . ? /`

Dilarang:
- U+2013 en dash dan U+2014 em dash
- U+2026 ellipsis, U+2022 bullet
- quotes lengkung U+2018 U+2019 U+201C U+201D
- arrow, star, check, dan semua simbol di luar daftar boleh
- huruf aksen dan non Latin

Ganti:
- `...` untuk ellipsis
- `-` untuk semua dash
- `"` dan `'` lurus untuk quotes

Cek dengan grep -P "[^\x00-\x7F]" dan perbaiki sampai PASS.

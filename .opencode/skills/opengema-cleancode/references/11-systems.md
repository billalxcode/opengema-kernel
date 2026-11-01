# Bab 11 - Systems

Sumber: Clean Code, bab 11.

## Inti

Sistem yang bersih memisahkan construction dari use. Startup dan wiring
tidak boleh tersebar di seluruh logika. Dependency harus eksplisit dan
mudah diganti untuk test.

Prinsip utama:

- Separate construction from use.
- Dependency injection.
- Factory untuk membuat object kompleks.
- Scaling dan decoupling.
- Test pada level sistem.
- Tunda keputusan arsitektur sampai ada bukti.
- Jangan mengunci sistem pada satu framework atau device.

## Aturan

- Composition root harus jelas dan kecil.
- Jangan memakai global singleton untuk dependency.
- Jangan menyembunyikan konstruksi di dalam fungsi bisnis.
- Setiap subsystem punya batas dan kontrak.
- System test harus mencakup boot, I/O, dan user mode.
- Failure pada satu subsystem tidak boleh menjatuhkan seluruh sistem.

## Penerapan di OpenGEMA

- `kernel/core/init.cpp` adalah composition root kernel.
- `kernel/core/init/{devices,filesystem,self_test,user}.cpp` memisahkan
  tahap startup.
- `init` user space adalah composition root untuk service dan shell.
- `ld-gem` adalah komponen sistem dynamic linking.
- `libgemapi` menyembunyikan `libkgema` dari aplikasi.
- Mount table dan VFS adalah subsystem dengan batas jelas.
- Jangan menambah dependency global baru di driver.
- System test: QEMU smoke, frame balance, shell, external app, GSLFS.
- Boot policy yang bisa berubah harus di user space, bukan kernel.

## Pertanyaan review

- Di mana construction dilakukan?
- Apakah dependency eksplisit?
- Apakah subsystem bisa dites terpisah?
- Apakah kegagalan satu subsystem terisolasi?
- Apakah ada global state yang tidak perlu?
- Apakah system test mencakup jalur penting?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 11.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 11:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter11.xhtml
- Catatan bab 11:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter11
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29

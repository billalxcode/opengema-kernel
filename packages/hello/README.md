# hello

Aplikasi eksternal pertama untuk menguji command lookup dari shell.

Shell mencari command di `/commands/<name>`, lalu di
`/applications/<name>/<name>`. `hello` berada di
`/applications/hello/hello` dan mencetak:

```
hello from applications
```

Binary ini dynamic seperti aplikasi lain:

```
PT_INTERP /system/boot/ld-gem
DT_NEEDED libgemapi.so.1
```

Manifest ada di `packages/system-root/applications/hello/manifest.txt`.

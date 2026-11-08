# system-root

Kerangka volume system GSLFS. CMake menyalin file ini ke
`build/system-root`, menambahkan binary hasil build, lalu
`scripts/mkgslimg.py` membuat `build/gema-system.img`.

Isi yang disalin dari sini:

```
system/config/version
system/config/gsl.conf
system/packages/registry
applications/gemabox/manifest.txt
```

Isi yang disalin dari build:

```
bin/gemabox
bin/echo
bin/cat
bin/ls
bin/pwd
bin/touch
bin/head
bin/wc
bin/grep
bin/free
bin/pkg
applications/hello/hello
applications/fstest/fstest
system/boot/init
system/boot/ld-gem
system/libraries/libgemapi.so.1
system/libraries/libkgema.so.1
```

Direktori kosong lain dibuat oleh CMake:

```
system/boot
system/libraries
system/services
system/commands
applications/gemabox/resources
commands
etc
users
tmp
var/log
var/run
var/cache
var/spool
dev
proc
volumes/boot
```

Image ditulis ke partisi GPT kedua oleh `scripts/make-esp.sh`. Root
di-mount read-only oleh kernel; `/dev`, `/proc`, `/tmp`, dan `/volumes/boot`
di-mount setelahnya.

Layout bersifat mutlak dan bagian dari ABI sistem. `gsl.conf` tidak boleh
mengubah mount point atau menambah path sistem. Volume tambahan hanya boleh
di bawah `/volumes`.

Layout lengkap ada di `docs/gsl/03-filesystem-layout.md`. Format image ada
di `docs/gsl/08-gslfs-format.md`.

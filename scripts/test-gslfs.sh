#!/bin/bash
# Host test for the GSLFS image builder and verifier. Runs without podman.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/root/system/config" "$TMP/root/applications/hello"
printf '0.1\n' > "$TMP/root/system/config/version"
printf 'hello from test\n' > "$TMP/root/applications/hello/hello"
chmod +x "$TMP/root/applications/hello/hello"

python3 "$ROOT/scripts/mkgslimg.py" "$TMP/root" "$TMP/a.img" >/dev/null
python3 "$ROOT/scripts/mkgslimg.py" --verify "$TMP/a.img" >/dev/null
python3 "$ROOT/scripts/mkgslimg.py" "$TMP/root" "$TMP/b.img" >/dev/null
cmp "$TMP/a.img" "$TMP/b.img"

CXX="${CXX:-clang++}"
"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I "$ROOT/libs/core" -I "$ROOT/kernel/include" \
  "$ROOT/tests/host/gslfs_probe.cpp" "$ROOT/kernel/fs/gslfs.cpp" \
  "$ROOT/libs/core/memory.cpp" \
  -o "$TMP/gslfs_probe"
"$TMP/gslfs_probe" "$TMP/a.img"

python3 - "$TMP/a.img" "$TMP/cycle.img" <<'PY'
import struct
import sys
import zlib

data = bytearray(open(sys.argv[1], "rb").read())
node_offset = struct.unpack_from("<Q", data, 32)[0]
node_count = struct.unpack_from("<I", data, 24)[0]
root = struct.unpack_from("<I", data, 28)[0]
for index in range(node_count):
    offset = node_offset + index * 56
    node_type = struct.unpack_from("<I", data, offset)[0]
    if index != root and node_type == 1:
        struct.pack_into("<I", data, offset + 16, index)
        crc = zlib.crc32(data[offset:offset + 48]) & 0xFFFFFFFF
        struct.pack_into("<I", data, offset + 48, crc)
        break
else:
    raise SystemExit("no directory node found")
open(sys.argv[2], "wb").write(data)
PY
"$TMP/gslfs_probe" "$TMP/cycle.img" --expect-mount-fail

python3 - "$TMP/a.img" "$TMP/corrupt.img" <<'PY'
import struct
import sys

data = bytearray(open(sys.argv[1], "rb").read())
data_offset = struct.unpack_from("<Q", data, 48)[0]
data[data_offset] ^= 0xFF
open(sys.argv[2], "wb").write(data)
PY
if python3 "$ROOT/scripts/mkgslimg.py" --verify "$TMP/corrupt.img" >/dev/null 2>&1; then
  echo "gslfs test: corrupt data accepted"
  exit 1
fi

printf 'not a gslfs image' > "$TMP/bad.img"
if python3 "$ROOT/scripts/mkgslimg.py" --verify "$TMP/bad.img" >/dev/null 2>&1; then
  echo "gslfs test: bad magic accepted"
  exit 1
fi

echo "gslfs test: OK"

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

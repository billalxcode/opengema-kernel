#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW_DIR="$ROOT/firmware"
CODE="$FW_DIR/AAVMF_CODE.fd"
VARS="$FW_DIR/AAVMF_VARS.fd"

if [ -f "$CODE" ] && [ -f "$VARS" ]; then
  echo "firmware exists:"
  ls -lh "$CODE" "$VARS"
  exit 0
fi

mkdir -p "$FW_DIR"

if [ "${OG_NATIVE:-0}" = "1" ]; then
  SRC_DIR="${AAVMF_DIR:-/usr/share/AAVMF}"
  if [ ! -f "$SRC_DIR/AAVMF_CODE.fd" ] || [ ! -f "$SRC_DIR/AAVMF_VARS.fd" ]; then
    echo "missing $SRC_DIR/AAVMF_CODE.fd or AAVMF_VARS.fd"
    echo "install qemu-efi-aarch64 or set AAVMF_DIR"
    exit 1
  fi
  cp "$SRC_DIR/AAVMF_CODE.fd" "$CODE"
  cp "$SRC_DIR/AAVMF_VARS.fd" "$VARS"
else
  echo "extract EDK2 firmware via podman (qemu-efi-aarch64) ..."
  podman run --rm -v "$ROOT:/work" -w /work opengema-build bash -c \
    "apt-get update -qq && apt-get install -y -qq qemu-efi-aarch64 >/dev/null 2>&1 && cp /usr/share/AAVMF/AAVMF_CODE.fd firmware/ && cp /usr/share/AAVMF/AAVMF_VARS.fd firmware/"
fi

ls -lh "$CODE" "$VARS"

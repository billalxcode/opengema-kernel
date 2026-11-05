#!/bin/bash
# Prepare the pinned AAVMF firmware used by the QEMU tests.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW_DIR="$ROOT/firmware"
CODE="$FW_DIR/AAVMF_CODE.fd"
VARS="$FW_DIR/AAVMF_VARS.fd"

# shellcheck source=scripts/firmware.lock
. "$ROOT/scripts/firmware.lock"

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

firmware_matches() {
  [ -f "$CODE" ] && [ -f "$VARS" ] || return 1
  [ "$(sha256_file "$CODE")" = "$FIRMWARE_CODE_SHA256" ] || return 1
  [ "$(sha256_file "$VARS")" = "$FIRMWARE_VARS_SHA256" ] || return 1
}

mkdir -p "$FW_DIR"

if firmware_matches; then
  echo "firmware: $FIRMWARE_VERSION"
  ls -lh "$CODE" "$VARS"
  exit 0
fi

if [ "${OG_NATIVE:-0}" = "1" ]; then
  bash "$ROOT/scripts/get-firmware-download.sh"
else
  podman run --rm -v "$ROOT:/work" -w /work opengema-build \
    bash scripts/get-firmware-download.sh
fi

if ! firmware_matches; then
  echo "firmware: checksum mismatch after download"
  exit 1
fi

echo "firmware: $FIRMWARE_VERSION"
ls -lh "$CODE" "$VARS"

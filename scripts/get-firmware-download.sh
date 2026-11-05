#!/bin/bash
# Download and verify the pinned AAVMF firmware package.
# Runs inside Linux, either natively or in the build container.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

# shellcheck source=scripts/firmware.lock
. "$ROOT/scripts/firmware.lock"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

ARCHIVE="$TMP/qemu-efi-aarch64.deb"
curl -fsSL "$FIRMWARE_URL" -o "$ARCHIVE"
printf '%s  %s\n' "$FIRMWARE_DEB_SHA256" "$ARCHIVE" | sha256sum -c -

dpkg-deb -x "$ARCHIVE" "$TMP/root"
install -m 0644 "$TMP/root/usr/share/AAVMF/AAVMF_CODE.fd" \
  firmware/AAVMF_CODE.fd
install -m 0644 "$TMP/root/usr/share/AAVMF/AAVMF_VARS.fd" \
  firmware/AAVMF_VARS.fd

printf '%s  %s\n' \
  "$FIRMWARE_CODE_SHA256" firmware/AAVMF_CODE.fd \
  "$FIRMWARE_VARS_SHA256" firmware/AAVMF_VARS.fd |
  sha256sum -c -

echo "firmware: downloaded $FIRMWARE_VERSION"

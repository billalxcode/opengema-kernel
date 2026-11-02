#!/bin/bash
# Full check: architecture rules, host unit tests, kernel build, QEMU smoke.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

echo "== architecture check =="
./scripts/check-arch.sh

echo "== host unit tests =="
./scripts/test-host.sh

echo "== kernel build =="
./scripts/build.sh

echo "== sdk package =="
./scripts/test-sdk.sh

echo "== esp image =="
pkill -f qemu-system-aarch64 2>/dev/null || true
sleep 1
./scripts/make-esp.sh

echo "== persistence test =="
./scripts/test-persist.sh

echo "== qemu smoke test =="
./scripts/test-qemu.sh

echo "== all checks passed =="

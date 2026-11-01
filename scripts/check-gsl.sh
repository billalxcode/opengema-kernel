#!/bin/bash
# GEMA System Layer guard.
# Fails when an application reaches below the public API, when the API leaks
# the kernel ABI, or when a syscall instruction appears outside libkgema.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

FAILED=0

fail() {
  echo "gsl-check: FAIL: $*"
  FAILED=1
}

# 1. Applications may only include the public gemapi headers.
APP_KGEMA="$(grep -RIn --include='*.cpp' --include='*.h' -E '#include[[:space:]]+"kgema/' packages/gemabox 2>/dev/null)"
if [ -n "$APP_KGEMA" ]; then
  echo "$APP_KGEMA"
  fail "packages/gemabox includes the private kernel ABI"
fi

APP_KERNEL="$(grep -RIn --include='*.cpp' --include='*.h' -E '#include[[:space:]]+"og/' packages/gemabox 2>/dev/null)"
if [ -n "$APP_KERNEL" ]; then
  echo "$APP_KERNEL"
  fail "packages/gemabox includes a kernel header"
fi

# 2. The public API headers must not expose kgema or kernel types.
API_LEAK="$(grep -RIn --include='*.h' -E '#include[[:space:]]+"(kgema|og)/' packages/libgemapi/include 2>/dev/null)"
if [ -n "$API_LEAK" ]; then
  echo "$API_LEAK"
  fail "packages/libgemapi/include leaks a lower layer header"
fi

# 3. The private ABI layer must not depend on the public API.
KGEMA_UP="$(grep -RIn --include='*.cpp' --include='*.h' -E '#include[[:space:]]+"gemapi/' packages/libkgema 2>/dev/null)"
if [ -n "$KGEMA_UP" ]; then
  echo "$KGEMA_UP"
  fail "packages/libkgema includes the public API"
fi

# 4. The kernel must not include user space headers.
KERNEL_USER="$(grep -RIn --include='*.cpp' --include='*.h' -E '#include[[:space:]]+"(gemapi|kgema)/' kernel libs 2>/dev/null)"
if [ -n "$KERNEL_USER" ]; then
  echo "$KERNEL_USER"
  fail "the kernel includes a user space header"
fi

# 5. Only libkgema may contain the svc instruction.
SVC_HITS="$(grep -RIn --include='*.cpp' --include='*.h' -w 'svc' packages 2>/dev/null \
  | grep -v '^packages/libkgema/src/syscall.cpp:')"
if [ -n "$SVC_HITS" ]; then
  echo "$SVC_HITS"
  fail "svc outside packages/libkgema/src/syscall.cpp"
fi

# 6. Applications must not provide the EL0 entry point.
ENTRY_HITS="$(grep -RIn --include='*.cpp' --include='*.h' '_start' packages/gemabox 2>/dev/null)"
if [ -n "$ENTRY_HITS" ]; then
  echo "$ENTRY_HITS"
  fail "packages/gemabox defines or references the entry point"
fi

# 7. The gemabox link line must name libgemapi only.
LINK_BLOCK="$(awk '/OUTPUT \$\{OG_GEMABOX_BIN\}/,/COMMENT "link gemabox"/' CMakeLists.txt 2>/dev/null)"
if [ -z "$LINK_BLOCK" ]; then
  fail "cannot find the gemabox link rule in CMakeLists.txt"
else
  if printf '%s\n' "$LINK_BLOCK" | grep -q 'KGEMA'; then
    fail "the gemabox link rule references libkgema directly"
  fi
  if ! printf '%s\n' "$LINK_BLOCK" | grep -q 'OG_GEMAPI_ARCHIVE'; then
    fail "the gemabox link rule does not use libgemapi"
  fi
fi

if [ "$FAILED" -ne 0 ]; then
  echo "gsl-check: FAILED"
  exit 1
fi

echo "gsl-check: OK"
exit 0

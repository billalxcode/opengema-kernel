#!/bin/bash
# Host compiler wrapper. Injects sanitizer and coverage flags without
# duplicating them in every test-host.sh compile line.

set -euo pipefail

REAL="${HOST_CXX:-clang++}"
FLAGS=()

if [ "${OG_SANITIZE:-0}" = "1" ]; then
  FLAGS+=(-fsanitize=address,undefined -fno-omit-frame-pointer
          -fno-sanitize-recover=all)
fi
if [ "${OG_COVERAGE:-0}" = "1" ]; then
  FLAGS+=(-fprofile-instr-generate -fcoverage-mapping -O0)
fi

exec "$REAL" "$@" "${FLAGS[@]}"

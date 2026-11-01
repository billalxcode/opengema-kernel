#!/bin/bash
# Architecture guard for opengema-kernel.
# Fails on layering violations, duplicated primitives, volatile outside io,
# and hardcoded board addresses.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

FAILED=0

fail() {
  echo "arch-check: FAIL: $*"
  FAILED=1
}

# Layer rank per source path. Higher may include lower only.
# kernel/core/init.cpp is the composition root: it is the one place allowed
# to pull every module together.
rank_of_path() {
  case "$1" in
    libs/*) echo 0 ;;
    kernel/arch/*) echo 1 ;;
    kernel/io/*) echo 2 ;;
    kernel/core/init.cpp) echo 15 ;;
    kernel/core/*) echo 3 ;;
    kernel/mem/*) echo 4 ;;
    kernel/events/*) echo 5 ;;
    kernel/time/*) echo 6 ;;
    kernel/dma/*) echo 7 ;;
    kernel/drivers/*) echo 8 ;;
    kernel/fs/*) echo 9 ;;
    kernel/sched/*) echo 10 ;;
    kernel/sync/*) echo 11 ;;
    kernel/process/*) echo 12 ;;
    kernel/syscall/*) echo 13 ;;
    kernel/platform/*) echo 14 ;;
    kernel/boot/*) echo 15 ;;
    *) echo -1 ;;
  esac
}

# Layer rank per public header. Foundational headers are 0 so that any
# layer may use them, as documented in the roadmap.
rank_of_header() {
  case "$1" in
    og/core/*) echo 0 ;;
    og/arch/*) echo 1 ;;
    og/io/*) echo 2 ;;
    og/mem/*) echo 4 ;;
    og/events/*) echo 5 ;;
    og/time/*) echo 6 ;;
    og/dma/*) echo 7 ;;
    og/drivers/*) echo 8 ;;
    og/fs/*) echo 9 ;;
    og/sched/*) echo 10 ;;
    og/sync/*) echo 11 ;;
    og/process/*) echo 12 ;;
    og/syscall/*) echo 13 ;;
    og/platform/*) echo 14 ;;
    *) echo -1 ;;
  esac
}

# 1. libs must not depend on kernel public headers.
LIB_HITS="$(grep -RIn --include='*.h' --include='*.cpp' -E '#include[[:space:]]+"og/' libs 2>/dev/null)"
if [ -n "$LIB_HITS" ]; then
  echo "$LIB_HITS"
  fail "libs/ includes a kernel header"
fi

# 2. Every og/ include must respect layer order.
while IFS= read -r file; do
  file_rank="$(rank_of_path "$file")"
  if [ "$file_rank" -lt 0 ]; then
    continue
  fi
  while IFS= read -r header; do
    if [ -z "$header" ]; then
      continue
    fi
    header_rank="$(rank_of_header "$header")"
    if [ "$header_rank" -lt 0 ]; then
      fail "$file includes unranked header $header"
      continue
    fi
    if [ "$header_rank" -gt "$file_rank" ]; then
      fail "$file (rank $file_rank) includes higher layer $header (rank $header_rank)"
    fi
  done < <(grep -oE '#include[[:space:]]+"og/[^"]+"' "$file" | sed -E 's/.*"(og\/[^"]+)"/\1/')
done < <(find kernel -type f \( -name '*.cpp' -o -name '*.h' \) | sort)

# 3. volatile outside kernel/io, except inline asm barriers.
while IFS= read -r line; do
  if [ -z "$line" ]; then
    continue
  fi
  case "$line" in
    *__asm__*volatile* | *asm*volatile*) continue ;;
  esac
  fail "volatile outside kernel/io: $line"
done < <(grep -RIn --include='*.cpp' --include='*.h' -w 'volatile' kernel 2>/dev/null \
  | grep -v '^kernel/io/' \
  | grep -vE ':[0-9]+:[[:space:]]*(//|\*|/\*)')

# 4. No second definition of memory or string primitives.
DUP_HITS="$(grep -RIn --include='*.cpp' -E '^(void\*|int|std::size_t)[[:space:]]+(memcpy|memmove|memset|memcmp|strlen|strcmp|strncmp)[[:space:]]*\(' kernel 2>/dev/null)"
if [ -n "$DUP_HITS" ]; then
  echo "$DUP_HITS"
  fail "duplicate memory primitive outside libs/core"
fi

# 5. Board addresses only in memory_map.h.
ADDR_HITS="$(grep -RIn --include='*.cpp' --include='*.h' -E '0x09000000|0x08000000|0x40000000' kernel 2>/dev/null \
  | grep -v '^kernel/include/og/platform/memory_map.h' \
  | grep -vE ':[0-9]+:[[:space:]]*(//|\*|/\*)')"
if [ -n "$ADDR_HITS" ]; then
  echo "$ADDR_HITS"
  fail "hardcoded board address outside memory_map.h"
fi

if [ "$FAILED" -ne 0 ]; then
  echo "arch-check: FAILED"
  exit 1
fi

echo "arch-check: OK"
exit 0

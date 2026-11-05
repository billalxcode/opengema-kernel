#!/bin/bash
# Line coverage for the core host test using LLVM source-based coverage.
# Writes the report to build/coverage/report.txt.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

COVERAGE_DIR="${OG_COVERAGE_DIR:-$ROOT/build/coverage}"
THRESHOLD="${OG_COVERAGE_MIN:-55}"
rm -rf "$COVERAGE_DIR"
mkdir -p "$COVERAGE_DIR/bin"

export LLVM_PROFILE_FILE="$COVERAGE_DIR/host_tests-%p.profraw"
CXX="$ROOT/scripts/host-cxx.sh" \
  OG_COVERAGE=1 \
  OG_HOST_CORE_ONLY=1 \
  OG_HOST_BUILD_DIR="$COVERAGE_DIR/bin" \
  ./scripts/test-host.sh

find_tool() {
  local name="$1"
  local candidate
  for candidate in "$name" "$name-19"; do
    if command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
  done
  echo "coverage: missing $name (or $name-19)" >&2
  return 1
}

PROFDATA="${LLVM_PROFDATA:-$(find_tool llvm-profdata)}"
LLVMCOV="${LLVM_COV:-$(find_tool llvm-cov)}"

PROFILE_FILES=("$COVERAGE_DIR"/*.profraw)
if [ ! -e "${PROFILE_FILES[0]}" ]; then
  echo "coverage: no profile data in $COVERAGE_DIR"
  exit 1
fi

"$PROFDATA" merge -sparse "${PROFILE_FILES[@]}" \
  -o "$COVERAGE_DIR/coverage.profdata"

IGNORE='(^|/)(tests|libs/cxx)/'
"$LLVMCOV" report "$COVERAGE_DIR/bin/host_tests" \
  -instr-profile="$COVERAGE_DIR/coverage.profdata" \
  -ignore-filename-regex="$IGNORE" | tee "$COVERAGE_DIR/report.txt"

PERCENT="$("$LLVMCOV" export -summary-only "$COVERAGE_DIR/bin/host_tests" \
  -instr-profile="$COVERAGE_DIR/coverage.profdata" \
  -ignore-filename-regex="$IGNORE" |
  python3 -c 'import json, sys; print(json.load(sys.stdin)["data"][0]["totals"]["lines"]["percent"])')"

echo "coverage: line percent $PERCENT, threshold $THRESHOLD"
awk -v percent="$PERCENT" -v threshold="$THRESHOLD" \
  'BEGIN { if (percent + 0 < threshold + 0) exit 1 }'

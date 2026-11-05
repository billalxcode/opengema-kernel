#!/bin/bash
# Run clang-tidy over every C++ file in compile_commands.json.
# Requires a completed build and clang-tidy on PATH.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

TIDY="${CLANG_TIDY:-clang-tidy}"
DB="build/compile_commands.json"

if [ ! -f "$DB" ]; then
  echo "clang-tidy: missing $DB, run ./scripts/build.sh first"
  exit 1
fi
if ! command -v "$TIDY" >/dev/null 2>&1; then
  echo "clang-tidy: $TIDY not found"
  exit 1
fi

python3 - "$TIDY" <<'PY'
import json
import subprocess
import sys

tidy = sys.argv[1]
with open("build/compile_commands.json", "r", encoding="ascii") as handle:
    entries = json.load(handle)

files = sorted({
    entry["file"]
    for entry in entries
    if entry["file"].endswith((".cpp", ".cc", ".cxx"))
})

failed = []
for path in files:
    result = subprocess.run(
        [tidy, "-p", "build", "--warnings-as-errors=*", path],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 0:
        failed.append(path)
        print("clang-tidy: FAIL " + path)
        print(result.stdout)

if failed:
    print("clang-tidy: %d of %d files failed" % (len(failed), len(files)))
    sys.exit(1)

print("clang-tidy: %d files OK" % len(files))
PY

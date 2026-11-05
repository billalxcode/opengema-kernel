#!/bin/bash
# Run the host test suite with AddressSanitizer and UndefinedBehaviorSanitizer.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

CXX="$ROOT/scripts/host-cxx.sh" OG_SANITIZE=1 "$ROOT/scripts/test-host.sh"

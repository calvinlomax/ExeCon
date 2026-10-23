#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
exec python3 tests/acceptance/run.py --build "${EXECON_BUILD_DIR:-build}" "$@"

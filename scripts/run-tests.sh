#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
ctest --test-dir "${EXECON_BUILD_DIR:-build}" --output-on-failure

#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
EXECON_BUILD_DIR=build-release EXECON_SKIP_APP=1 scripts/build.sh -DCMAKE_BUILD_TYPE=Release
build-release/execon_bench

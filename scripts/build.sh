#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir="${EXECON_BUILD_DIR:-build}"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug "$@"
cmake --build "$build_dir" --parallel
if [[ "$(uname -sm)" == "Darwin arm64" ]]; then
  scripts/sign.sh "$build_dir/execon"
  if [[ -f app/ExeCon.xcodeproj/project.pbxproj ]]; then
    scripts/build-app.sh "$build_dir"
  fi
fi

#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir="${EXECON_BUILD_DIR:-build}"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug "$@"
cmake --build "$build_dir" --parallel
if [[ "$(uname -sm)" == "Darwin arm64" ]]; then
  scripts/sign.sh "$build_dir/execon"
  for test_binary in execon_tests execon_diff execon_bench execon_api_test; do
    if [[ -f "$build_dir/$test_binary" ]]; then scripts/sign.sh "$build_dir/$test_binary"; fi
  done
  if [[ -f app/ExeCon.xcodeproj/project.pbxproj ]] && ! grep -q "EXECON_SANITIZE:BOOL=ON" "$build_dir/CMakeCache.txt" && [[ "${EXECON_SKIP_APP:-0}" != 1 ]]; then
    scripts/build-app.sh "$build_dir"
  fi
fi

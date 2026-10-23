#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir="${EXECON_BUILD_DIR:-build}"
ctest --test-dir "$build_dir" --output-on-failure
if [[ "${1:-}" == "--unit" ]]; then exit 0; fi
scripts/format.sh
python3 tests/unit/test_downloads.py
scripts/build-guests.sh
sanitized=0
if grep -q 'EXECON_SANITIZE:BOOL=ON' "$build_dir/CMakeCache.txt"; then
  sanitized=1
  python3 tests/integration/run.py "$build_dir" --interp-only
else
  python3 tests/integration/run.py "$build_dir"
fi
"$build_dir/execon_api_test" "$build_dir/guests/exit-x86_64.exe"
"$build_dir/execon_api_test" "$build_dir/guests/exit-i686.exe"
python="${EXECON_ORACLE_PYTHON:-.dev-tools/venv/bin/python}"
[[ -x "$python" ]] || { echo 'Oracle tools missing: run scripts/bootstrap-dev-tools.sh' >&2; exit 1; }
"$python" -u tests/diff/oracle.py "$build_dir/execon_diff"
if [[ "$(uname -sm)" == "Darwin arm64" && $sanitized == 0 ]]; then
  "$python" -u tests/diff/oracle.py "$build_dir/execon_diff" --backend jit
  mkdir -p "$build_dir/swift-module-cache"
  swiftc -warnings-as-errors -target arm64-apple-macos14.0 -module-cache-path "$build_dir/swift-module-cache" \
    app/ConsoleDecoder.swift tests/app/main.swift -o "$build_dir/execon_console_tests"
  "$build_dir/execon_console_tests"
  "$build_dir/execon" --self-check
  if [[ -d "$build_dir/ExeCon.app" ]]; then
    codesign --verify --deep --strict "$build_dir/ExeCon.app"
    "$build_dir/ExeCon.app/Contents/Helpers/execon" --self-check
  fi
else
  echo 'SKIP native oracle and Swift app checks: portable/sanitizer configuration'
fi
printf '%s\n' 'Supported-subset checks passed. Full Windows compatibility is not claimed.' \
  'Third-party acceptance is a separate, currently failing gate: scripts/run-acceptance.sh'

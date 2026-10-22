#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -sm)" == "Darwin arm64" ]] || { echo 'Signing requires Darwin arm64' >&2; exit 1; }
codesign --force --sign "${EXECON_SIGN_IDENTITY:--}" --options runtime --entitlements config/jit.entitlements "$1"

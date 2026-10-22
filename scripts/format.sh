#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
formatter="${EXECON_CLANG_FORMAT:-.dev-tools/venv/bin/clang-format}"
command -v "$formatter" >/dev/null || { echo 'clang-format missing; install dev tools or set EXECON_CLANG_FORMAT' >&2; exit 1; }
find engine cli tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 "$formatter" "${@:---dry-run}" --Werror

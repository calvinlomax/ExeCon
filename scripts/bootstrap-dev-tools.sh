#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m venv .dev-tools/venv
.dev-tools/venv/bin/python -m pip install clang-format==18.1.8 unicorn==2.1.4 capstone==5.0.7
printf '%s\n' 'Developer tools are isolated and never linked into product binaries.' 'Install llvm-mingw separately and set EXECON_MINGW_ROOT to its prefix for compiler-built guests.'
if [[ "${1:-}" == "--mingw" ]]; then
  archive=.dev-tools/llvm-mingw.tar.xz
  curl --fail --location --proto '=https' --tlsv1.2 https://github.com/mstorsjo/llvm-mingw/releases/download/20261020/llvm-mingw-20261020-ucrt-macos-universal.tar.xz -o "$archive"
  python3 - "$archive" <<'PY'
import hashlib,sys
with open(sys.argv[1], 'rb') as f:
    actual=hashlib.file_digest(f, 'sha256').hexdigest()
if actual != 'e9c13e4ede38e715003ffc674d4d00a1088df8235439d1f0c16188ff0704c17e':
    raise SystemExit('llvm-mingw SHA-256 mismatch; refusing extraction')
PY
  tar -xJf "$archive" -C .dev-tools
fi

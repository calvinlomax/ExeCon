#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m venv .dev-tools/venv
.dev-tools/venv/bin/python -m pip install clang-format==18.1.8 unicorn==2.1.4 capstone==5.0.7
printf '%s\n' 'Developer tools are isolated and never linked into product binaries.' 'Install llvm-mingw separately and set EXECON_MINGW_ROOT to its prefix for compiler-built guests.'

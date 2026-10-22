#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mingw="${EXECON_MINGW_ROOT:-$PWD/.dev-tools/llvm-mingw-20261020-ucrt-macos-universal}"
build_dir="${EXECON_BUILD_DIR:-build}"
[[ -x "$mingw/bin/x86_64-w64-mingw32-clang" ]] || { echo 'llvm-mingw unavailable: set EXECON_MINGW_ROOT. Compiler-built guest tests cannot run.' >&2; exit 1; }
mkdir -p "$build_dir/guests"
for arch in x86_64 i686; do
  for source in tests/guests/*.c; do
    name="$(basename "$source" .c)"
    "$mingw/bin/$arch-w64-mingw32-clang" -O1 -fno-stack-protector -fno-builtin -mno-sse -mno-sse2 -nostdlib -Wl,--entry,mainCRTStartup -Wl,--subsystem,console "$source" -lkernel32 -luser32 -o "$build_dir/guests/$name-$arch.exe"
  done
done

#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mingw="${EXECON_MINGW_ROOT:-$PWD/.dev-tools/llvm-mingw-20261020-ucrt-macos-universal}"
build_dir="${EXECON_BUILD_DIR:-build}"
mkdir -p "$build_dir/runtime-guests"
failed=0
for arch in x86_64 i686; do
  for source in tests/guests/runtime/*.c tests/guests/runtime/*.cpp; do
    name="$(basename "${source%.*}")"
    compiler=clang
    if [[ "$source" == *.cpp ]]; then compiler=clang++; fi
    if ! "$mingw/bin/$arch-w64-mingw32-$compiler" -O1 -Wall -Wextra -Werror -fms-extensions -static \
      "$source" -ladvapi32 -o "$build_dir/runtime-guests/$name-$arch.exe" 2>"$build_dir/runtime-guests/$name-$arch.build-log"; then
      echo "Compiler blocked $name/$arch; see $build_dir/runtime-guests/$name-$arch.build-log" >&2
      failed=1
    fi
  done
done

exit "$failed"

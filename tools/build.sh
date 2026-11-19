#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$BASH_SOURCE")/.." && pwd)"
toolchain_root="${MINGW32_ROOT:-}"
if [ -z "$toolchain_root" ]; then
  toolchain_root=/home/mrfox/Library/dev/toolchains/mingw32
fi
export PATH="$toolchain_root/usr/bin:$PATH"
build_dir="$root/build"
if [ "$#" -ge 1 ]; then
  build_dir="$1"
fi

cmake -S "$root" -B "$build_dir" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw32.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir"

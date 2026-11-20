#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
. "$HOME/Library/.msvc/activate-x86.sh"
export PATH="$HOME/Library/dev/toolchains/nasm/usr/bin:$PATH"
build_dir="$root/build"
if [ "$#" -ge 1 ]; then
  build_dir="$1"
fi

cmake -S "$root" -B "$build_dir" -G Ninja \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_SYSTEM_PROCESSOR=x86 \
  -DCMAKE_C_COMPILER=cl \
  -DCMAKE_CXX_COMPILER=cl \
  -DCMAKE_RC_COMPILER=rc \
  -DCMAKE_ASM_NASM_COMPILER=nasm \
  -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded \
  -DCMAKE_BUILD_TYPE=Release \
  -DTTP_ENABLE_RONAN=ON
cmake --build "$build_dir"

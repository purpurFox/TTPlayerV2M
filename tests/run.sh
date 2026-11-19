#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$BASH_SOURCE")/.." && pwd)"
cache="$HOME/.cache/ttpv2m"
mkdir -p "$cache"
dll="$root/build/ttp_v2m.dll"
host="$root/build/ttp_v2m_test.exe"
if [ ! -f "$dll" ] || [ ! -f "$host" ]; then
  "$root/tools/build.sh"
fi
for name in pzero_new v2_zeitmaschine_new; do
  source_file="$root/tests/data/$name.v2m"
  output_file="$cache/$name.wav"
  echo "== $name =="
  WINEDEBUG=-all wine "$host" \
    "$(winepath -w "$dll")" \
    "$(winepath -w "$source_file")" \
    "$(winepath -w "$output_file")"
done

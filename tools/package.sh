#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$BASH_SOURCE")/.." && pwd)"
"$root/tools/build.sh"
mkdir -p "$root/dist"
cp "$root/build/ttp_v2m.dll" "$root/dist/ttp_v2m.dll"
cd "$root/dist"
sha256sum ttp_v2m.dll > SHA256SUMS.txt
echo "wrote $root/dist/ttp_v2m.dll"
cat SHA256SUMS.txt

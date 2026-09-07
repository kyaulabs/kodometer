#!/usr/bin/env bash
set -euo pipefail
[[ "$(uname -m)" == x86_64 ]] || { echo 'packaging: x64 only' >&2; exit 1; }
repo_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
cd "$repo_root"
git diff --quiet
git diff --cached --quiet
version="$(python3 scripts/package_metadata.py version)"
mkdir -p dist/source
git archive --format=tar --prefix="kodometer-$version/" HEAD | gzip -n >"dist/source/kodometer-$version-source.tar.gz"
printf '%s\n' "dist/source/kodometer-$version-source.tar.gz"

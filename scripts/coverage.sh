#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
build_dir="${CODEXBAR_COVERAGE_BUILD_DIR:-$repo_root/build-coverage}"

command -v gcovr >/dev/null || {
    echo "coverage: gcovr is required" >&2
    exit 127
}

rm -rf "$build_dir" "$repo_root/coverage"
cmake \
    -S "$repo_root" \
    -B "$build_dir" \
    -G Ninja \
    -DBUILD_TESTING=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCODEXBAR_BUILD_APPLET=OFF \
    -DCODEXBAR_ENABLE_COVERAGE=ON
cmake --build "$build_dir"
ctest --test-dir "$build_dir" --output-on-failure
mkdir -p "$repo_root/coverage"
(
    cd "$repo_root"
    gcovr
)

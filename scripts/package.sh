#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
build_dir="${KODOMETER_PACKAGE_BUILD_DIR:-$repo_root/build-package}"
staging_dir="$build_dir/staging"
dist_dir="${KODOMETER_DIST_DIR:-$repo_root/dist}"
version="$(awk '/^[[:space:]]*VERSION [0-9]+\.[0-9]+\.[0-9]+/{print $2; exit}' "$repo_root/CMakeLists.txt")"
architecture="$(uname -m)"
artifact="kodometer-$version-linux-$architecture.tar.gz"

rm -rf "$build_dir"
mkdir -p "$staging_dir" "$dist_dir"
cmake \
    -S "$repo_root" \
    -B "$build_dir" \
    -G Ninja \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DKODOMETER_BUILD_APPLET=ON
cmake --build "$build_dir"
DESTDIR="$staging_dir" cmake --install "$build_dir"

epoch="${SOURCE_DATE_EPOCH:-$(git -C "$repo_root" log -1 --format=%ct)}"
tar \
    --create \
    --gzip \
    --file "$dist_dir/$artifact" \
    --directory "$staging_dir" \
    --sort=name \
    --mtime="@$epoch" \
    --owner=0 \
    --group=0 \
    --numeric-owner \
    .
(
    cd "$dist_dir"
    sha256sum "$artifact" >"$artifact.sha256"
)
printf '%s\n' "$dist_dir/$artifact"

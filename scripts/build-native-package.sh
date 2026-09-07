#!/usr/bin/env bash
set -euo pipefail
[[ "$(uname -m)" == x86_64 ]] || { echo 'packaging: x64 only' >&2; exit 1; }
repo_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
cd "$repo_root"
target="${1:?packaging: target is required}"
case "$target" in arch | ubuntu-26.04 | fedora-43 | fedora-44) ;; *) exit 1 ;; esac
# shellcheck source=/dev/null
source /etc/os-release
[[ "$target" == "$ID-${VERSION_ID:-rolling}" || ( "$target" == arch && "$ID" == arch ) ]]
version="$(python3 scripts/package_metadata.py version)"
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct)}"
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
work="$(mktemp -d "$repo_root/build-native-$target.XXXXXX")"
out="$repo_root/dist/$target"
mkdir -p "$out"
printf 'packaging: build directory %s\n' "$work"

if [[ "$target" == arch ]]; then
    source_archive="$repo_root/dist/source/kodometer-$version-source.tar.gz"
    cp "$source_archive" "$work/"
    python3 scripts/package_metadata.py pkgbuild "$source_archive" >"$work/PKGBUILD"
    # makepkg must never run as root. Package code has no sudo access during builds.
    if [[ "$EUID" == 0 ]]; then
        id packagebuilder >/dev/null 2>&1 || useradd --create-home packagebuilder
        chown -R packagebuilder:packagebuilder "$work"
        builder=(runuser -u packagebuilder --)
    else
        builder=()
    fi
    (
        cd "$work"
        "${builder[@]}" makepkg --printsrcinfo > .SRCINFO
        "${builder[@]}" env CMAKE_BUILD_PARALLEL_LEVEL="$CMAKE_BUILD_PARALLEL_LEVEL" \
            SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software makepkg --cleanbuild --noconfirm
    )
    cp "$work/kodometer-$version-1-x86_64.pkg.tar.zst" "$out/"
    tar --create --gzip --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner \
        --file "$out/kodometer-$version-aur.tar.gz" --directory "$work" PKGBUILD .SRCINFO
    tar --create --gzip --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner \
        --file "$out/kodometer-$version-linux-x86_64.tar.gz" --directory "$work/pkg/kodometer" ./usr
else
    if [[ "$target" == ubuntu-* ]]; then
        export DEB_BUILD_MAINT_OPTIONS=hardening=+all
        CXXFLAGS="$(dpkg-buildflags --get CPPFLAGS) $(dpkg-buildflags --get CXXFLAGS)"
        LDFLAGS="$(dpkg-buildflags --get LDFLAGS)"
    else
        CXXFLAGS="$(rpm --eval '%{build_cxxflags}')"
        LDFLAGS="$(rpm --eval '%{build_ldflags}')"
    fi
    export CXXFLAGS LDFLAGS
    cmake -S . -B "$work" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_TESTING=ON -DCMAKE_COMPILE_WARNING_AS_ERROR=ON -DKODOMETER_PACKAGE_TARGET="$target"
    cmake --build "$work"
    ctest --test-dir "$work" --output-on-failure
    cpack --config "$work/CPackConfig.cmake"
    if [[ "$target" == ubuntu-* ]]; then
        artifact="kodometer_$version-1ubuntu$VERSION_ID"_amd64.deb
        cp "$work/packages/$artifact" "$out/"
    else
        artifact="kodometer-$version-1.fc$VERSION_ID.x86_64.rpm"
        cp "$work/packages/$artifact" "$out/"
    fi
fi

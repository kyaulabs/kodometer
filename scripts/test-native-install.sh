#!/usr/bin/env bash
# Fresh CI runtime containers: install only declared runtime dependencies, not SDKs.
set -euo pipefail
[[ "$EUID" == 0 && "$(uname -m)" == x86_64 ]]
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
version="$(python3 scripts/package_metadata.py version)"
target="${1:?expected package target}"
case "$target" in
    arch)
        pacman -Syu --needed --noconfirm dbus procps-ng
        package="$root/dist/install/kodometer-$version-1-x86_64.pkg.tar.zst"
        pacman -U --noconfirm "$package"
        pacman -U --noconfirm "$package"
        plugin="$(pacman -Qlq kodometer | grep '/plasma/applets/org.kyaulabs.kodometer.so$')"
        ;;
    ubuntu-26.04)
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y --no-install-recommends dbus procps
        package="$root/dist/install/kodometer_$version-1ubuntu26.04"_amd64.deb
        [[ "$(dpkg-deb -f "$package" Architecture)" == amd64 ]]
        dpkg-deb -f "$package" Depends
        apt-get install -y --no-install-recommends "$package"
        apt-get install -y --no-install-recommends --reinstall "$package"
        plugin="$(dpkg-query -L kodometer | grep '/plasma/applets/org.kyaulabs.kodometer.so$')"
        ;;
    fedora-43 | fedora-44)
        dnf install -y dbus-daemon procps-ng
        package="$root/dist/install/kodometer-$version-1.fc${target#fedora-}.x86_64.rpm"
        [[ "$(rpm -qp --qf '%{ARCH}' "$package")" == x86_64 ]]
        rpm -qp --requires "$package"
        dnf install -y "$package"
        dnf reinstall -y "$package"
        plugin="$(rpm -ql kodometer | grep '/plasma/applets/org.kyaulabs.kodometer.so$')"
        ;;
    *) exit 1 ;;
esac
scripts/smoke-package.sh "$plugin"

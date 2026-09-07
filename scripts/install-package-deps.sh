#!/usr/bin/env bash
# Build-container provisioning, not an installer for users' desktops.
set -euo pipefail
[[ "$(uname -m)" == x86_64 ]] || { echo 'packaging: x64 only' >&2; exit 1; }
[[ "$EUID" == 0 ]] || { echo 'packaging: run provisioning inside a root build container' >&2; exit 1; }
# shellcheck source=/dev/null
source /etc/os-release
case "${1:-}" in
    arch)
        [[ "$ID" == arch ]]
        pacman -Syu --needed --noconfirm base-devel cmake dbus extra-cmake-modules git jq \
            kconfig kcoreaddons kirigami knotifications kwallet libplasma ninja \
            plasma-workspace python qt6-declarative sudo
        ;;
    ubuntu-26.04)
        [[ "$ID" == ubuntu && "$VERSION_ID" == 26.04 ]]
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y --no-install-recommends build-essential cmake dbus dpkg-dev \
            extra-cmake-modules file git jq libkf6config-dev libkf6coreaddons-dev \
            libkf6notifications-dev libkf6wallet-dev libkirigami-dev libplasma-dev \
            ninja-build plasma-workspace procps python3 qt6-declarative-dev \
            qt6-declarative-dev-tools qml6-module-org-kde-kirigami qml6-module-qttest \
            qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript \
            qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
            qml6-module-qtquick-layouts qml6-module-qtquick-window
        ;;
    fedora-43 | fedora-44)
        [[ "$ID" == fedora && "fedora-$VERSION_ID" == "$1" ]]
        dnf install -y cmake dbus-daemon extra-cmake-modules gcc-c++ git jq \
            kf6-kconfig-devel kf6-kcoreaddons-devel kf6-kirigami-devel \
            kf6-knotifications-devel kf6-kwallet-devel libplasma-devel ninja-build \
            plasma-workspace procps-ng python3 qt6-qtdeclarative-devel \
            redhat-rpm-config rpm-build
        ;;
    *) echo 'packaging: unsupported target' >&2; exit 1 ;;
esac

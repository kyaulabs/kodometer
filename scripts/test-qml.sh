#!/usr/bin/env bash
set -euo pipefail

# Color dialogs and Plasma tooltips must not read/write the desktop's settings
# or share its D-Bus session, even when tests use the offscreen platform.
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work"/{home,config,data,cache,runtime}
chmod 700 "$work/runtime"
export HOME="$work/home"
export XDG_CONFIG_HOME="$work/config"
export XDG_DATA_HOME="$work/data"
export XDG_CACHE_HOME="$work/cache"
export XDG_RUNTIME_DIR="$work/runtime"
export QT_QPA_PLATFORM=offscreen
# Qt must not activate desktop portals or leave a document-portal mount behind.
export QT_NO_XDG_DESKTOP_PORTAL=1
dbus-run-session -- "$@"

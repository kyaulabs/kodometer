#!/usr/bin/env bash
# Only launch the supplied, already-installed plugin in an isolated test session.
set -euo pipefail
plugin="$(realpath "${1:?smoke: expected installed plugin path}")"
[[ -f "$plugin" && "$plugin" == */plasma/applets/org.kyaulabs.kodometer.so ]]
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
ldd -r "$plugin" >"$work/linkage"
if grep -E 'not found|undefined symbol' "$work/linkage"; then exit 1; fi
mkdir -p "$work/home/.config" "$work/runtime"
chmod 700 "$work/runtime"
printf '[Wallet]\nEnabled=false\n' >"$work/home/.config/kwalletrc"
printf '[Applets][4][Configuration][General]\nautoRefresh=false\ndisabledProviders=codex,claude,gemini,openrouter,xai,zai,kimi\ndeepseekAccountId=11111111-1111-4111-8111-111111111111\n' >"$work/home/.config/plasmawindowedrc"
plugin_dir="$(dirname "$(dirname "$(dirname "$plugin")")")"
if ! env -i HOME="$work/home" XDG_CONFIG_HOME="$work/home/.config" XDG_CACHE_HOME="$work/home/.cache" \
    XDG_RUNTIME_DIR="$work/runtime" XDG_DATA_DIRS=/usr/share PATH=/usr/bin:/bin \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_PLUGIN_PATH="$plugin_dir" \
    PLUGIN="$plugin" MAP_LOG="$work/maps" dbus-run-session -- bash >"$work/startup" 2>&1 <<'CHILD'
timeout 12s plasmawindowed org.kyaulabs.kodometer & timer=$!
sleep 4
child=$(pgrep -P "$timer" plasmawindowed)
grep -F "$PLUGIN" "/proc/$child/maps" > "$MAP_LOG"
loaded=$?
wait "$timer"
result=$?
test "$loaded" -eq 0 && test "$result" -eq 124
CHILD
then
    cat "$work/startup"
    exit 1
fi
if grep -E 'ReferenceError|TypeError|Cannot assign|is not a type|failed to load|module .* is not installed|qrc:' "$work/startup"; then exit 1; fi
printf 'smoke: installed plugin loaded successfully: %s\n' "$plugin"

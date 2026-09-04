#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
cd "$repo_root"

mapfile -d '' cpp_files < <(
    find include src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 | sort -z
)
clang-format --dry-run --Werror "${cpp_files[@]}"

qmlformat="${QMLFORMAT:-}"
if [[ -z "$qmlformat" ]]; then
    qmlformat="$(command -v qmlformat || true)"
fi
if [[ -z "$qmlformat" ]] && command -v qmake6 >/dev/null; then
    qmlformat="$(qmake6 -query QT_INSTALL_BINS)/qmlformat"
fi
if [[ ! -x "$qmlformat" ]]; then
    echo "format: qmlformat is required" >&2
    exit 127
fi

mapfile -d '' qml_files < <(find applet tests/qml -type f -name '*.qml' -print0 | sort -z)
temporary_directory="$(mktemp -d)"
trap 'rm -rf "$temporary_directory"' EXIT
for file in "${qml_files[@]}"; do
    formatted="$temporary_directory/$(printf '%s' "$file" | tr '/' '_')"
    "$qmlformat" "$file" >"$formatted"
    diff --unified "$file" "$formatted"
done

git diff --check

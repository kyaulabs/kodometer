#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
release_branch="${1:-${GITHUB_HEAD_REF:-}}"

if [[ ! "$release_branch" =~ ^release/([0-9]+\.[0-9]+\.[0-9]+)$ ]]; then
    echo "release: expected a release/X.Y.Z branch, got '$release_branch'" >&2
    exit 1
fi

branch_version="${BASH_REMATCH[1]}"
cmake_version="$(awk '/^[[:space:]]*VERSION [0-9]+\.[0-9]+\.[0-9]+/{print $2; exit}' "$repo_root/CMakeLists.txt")"
metadata_version="$(jq -r '.KPlugin.Version' "$repo_root/applet/metadata.json")"
package_version="$(jq -r '.version' "$repo_root/package.json")"

for version in "$cmake_version" "$metadata_version" "$package_version"; do
    if [[ "$version" != "$branch_version" ]]; then
        echo "release: version '$version' does not match branch version '$branch_version'" >&2
        exit 1
    fi
done

printf '%s\n' "$branch_version"

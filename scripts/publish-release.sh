#!/usr/bin/env bash
# CI/maintainer automation only. Never called by the applet.
set -euo pipefail

repo_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
cd "$repo_root"
version="$(scripts/validate-release.sh "${1:-}")"
tag="v$version"
: "${GH_TOKEN:?release: GH_TOKEN is required}"
: "${BOT_TOKEN:?release: BOT_TOKEN is required for back-merge pull requests}"
: "${RELEASE_COMMIT:?release: RELEASE_COMMIT is required}"
: "${GITHUB_REPOSITORY:?release: GITHUB_REPOSITORY is required}"
repo="$GITHUB_REPOSITORY"

fail() {
    printf 'release: %s\n' "$*" >&2
    exit 1
}

[[ "$repo" =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$ ]] || fail 'invalid repository name'
[[ "$RELEASE_COMMIT" =~ ^[0-9a-f]{40}$ ]] || fail 'expected a full merge commit SHA'
[[ "$(git rev-parse HEAD)" == "$RELEASE_COMMIT" ]] || fail 'checkout does not match the release merge commit'
git diff --quiet
git diff --cached --quiet

# Fail on API/authentication errors rather than treating every error as "not found".
GH_TOKEN="$BOT_TOKEN" gh pr list --repo "$repo" --base develop --head main --state open --json number >/dev/null
state="$(gh api --paginate "repos/$repo/releases" --jq \
    ".[] | select(.tag_name == \"$tag\") | if .prerelease then \"prerelease\" elif .draft then \"draft\" else \"published\" end")"
case "$state" in
    '' | draft | published) ;;
    *) fail 'unexpected or ambiguous release state' ;;
esac

has_tag=false
if git show-ref --verify --quiet "refs/tags/$tag"; then
    has_tag=true
    [[ "$(git cat-file -t "refs/tags/$tag")" == tag ]] || fail 'existing release tag is not annotated'
    [[ "$(git rev-parse "refs/tags/$tag^{commit}")" == "$RELEASE_COMMIT" ]] || fail 'existing tag points to another commit'
elif [[ "$state" == published ]]; then
    fail 'published release has no matching local tag; fetch tags before retrying'
fi

artifact="kodometer-$version-linux-$(uname -m).tar.gz"
dist_dir="${KODOMETER_DIST_DIR:-$repo_root/dist}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

verify_pair() {
    local directory="$1"
    local expected
    [[ -f "$directory/$artifact" && -f "$directory/$artifact.sha256" ]] || fail 'release archive or checksum is missing'
    expected="$(cd "$directory" && sha256sum -- "$artifact")"
    [[ "$(<"$directory/$artifact.sha256")" == "$expected" ]] || fail 'release checksum does not match its archive and filename'
}

if [[ "$state" != published ]]; then
    verify_pair "$dist_dir"
    # Generate notes before creating a tag, so a changelog failure has no remote effect.
    if [[ -f "docs/releases/$version.md" ]]; then
        cat "docs/releases/$version.md" >"$work/notes.md"
        printf '\n\n' >>"$work/notes.md"
    fi
    git-cliff --latest --use-branch-tags --tag "$tag" >>"$work/notes.md"
    if [[ "$has_tag" == false ]]; then
        git config user.name 'github-actions[bot]'
        git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
        git tag --annotate "$tag" --message "Kodometer $tag"
    fi
    # Also retries a previously failed push of an already-created local tag. Never force.
    git push origin "refs/tags/$tag"
    if [[ -z "$state" ]]; then
        gh release create "$tag" --repo "$repo" --draft --verify-tag \
            --title "Kodometer $tag" --notes-file "$work/notes.md"
    fi
    [[ "$(gh release view "$tag" --repo "$repo" --json isDraft --jq '.isDraft')" == true ]] || fail 'release is no longer a draft; refusing to replace assets'
    gh release upload "$tag" "$dist_dir/$artifact" "$dist_dir/$artifact.sha256" --repo "$repo" --clobber
fi

mkdir "$work/download"
gh release download "$tag" --repo "$repo" --dir "$work/download" \
    --pattern "$artifact" --pattern "$artifact.sha256"
verify_pair "$work/download"
if [[ "$state" != published ]]; then
    cmp -- "$dist_dir/$artifact" "$work/download/$artifact"
    cmp -- "$dist_dir/$artifact.sha256" "$work/download/$artifact.sha256"
    gh release edit "$tag" --repo "$repo" --draft=false
fi

# Published assets are immutable. A later rolling-toolchain rebuild may differ;
# verify the published pair itself, then resume only the back-merge operation.
existing="$(GH_TOKEN="$BOT_TOKEN" gh pr list --repo "$repo" \
    --base develop --head main --state open --json number --jq '.[0].number // empty')"
if [[ -n "$existing" ]]; then
    printf 'release: back-merge pull request #%s already exists\n' "$existing"
    exit 0
fi
cat >"$work/back-merge.md" <<EOF
## Summary

Back-merge release $tag from \`main\` into \`develop\` to retain release history.

## Verification

- Version metadata matched the release branch.
- Annotated tag \`$tag\` points to merge commit \`$RELEASE_COMMIT\`.
- The published archive and checksum were downloaded and verified.
EOF
GH_TOKEN="$BOT_TOKEN" gh pr create --repo "$repo" --base develop --head main \
    --title "chore(release): back-merge $tag into develop" --body-file "$work/back-merge.md"

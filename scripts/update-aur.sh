#!/usr/bin/env bash
# CI only: consume recipes already verified against the published release.
set -euo pipefail
umask 077
fail() { printf 'aur: %s\n' "$*" >&2; exit 1; }
recipes="$(realpath "${1:?expected verified recipe directory}")"
version="${2:?expected release version}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
for name in AUR_SSH_PRIVATE_KEY AUR_SSH_KNOWN_HOSTS AUR_GPG_PRIVATE_KEY AUR_SIGNING_KEY AUR_COMMIT_NAME AUR_COMMIT_EMAIL; do
    [[ -n "${!name:-}" ]] || fail "$name is not configured"
done
fingerprint="${AUR_SIGNING_KEY^^}"
[[ "$fingerprint" =~ ^[0-9A-F]{40}$ ]] || fail 'expected a full signing-key fingerprint'
work="$(mktemp -d)"
cleanup() {
    GNUPGHOME="$work/gnupg" gpgconf --kill gpg-agent >/dev/null 2>&1 || true
    rm -rf -- "$work"
}
trap cleanup EXIT
printf '%s\n' "$AUR_SSH_PRIVATE_KEY" >"$work/ssh-key"
printf '%s\n' "$AUR_SSH_KNOWN_HOSTS" >"$work/known-hosts"
printf -v GIT_SSH_COMMAND 'ssh -i %q -o IdentitiesOnly=yes -o BatchMode=yes -o PasswordAuthentication=no -o StrictHostKeyChecking=yes -o UserKnownHostsFile=%q' "$work/ssh-key" "$work/known-hosts"
export GIT_SSH_COMMAND GIT_TERMINAL_PROMPT=0
git clone --single-branch ssh://aur@aur.archlinux.org/kodometer.git "$work/aur"

state="$(python3 - "$work/aur" "$recipes" "$version" "$script_dir" <<'PY'
from pathlib import Path
import re
import sys
sys.path.insert(0, sys.argv[4])
from package_metadata import checked_version
repository, recipes = map(Path, sys.argv[1:3])
version = checked_version(sys.argv[3])

def files(directory):
    names = {path.name for path in directory.iterdir()} - {'.git'}
    if names != {'PKGBUILD', '.SRCINFO'}:
        raise ValueError('expected only PKGBUILD and .SRCINFO; inspect the AUR repository manually')
    for name in names:
        path = directory / name
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 65536:
            raise ValueError('expected bounded regular AUR recipes')
    return {name: (directory / name).read_bytes() for name in names}

def pkg_version(content):
    text = content['.SRCINFO'].decode('utf-8')
    for key, expected in [('pkgbase', 'kodometer'), ('pkgname', 'kodometer'), ('arch', 'x86_64')]:
        if re.findall(r'^\s*' + key + r' = (.+)$', text, re.MULTILINE) != [expected]:
            raise ValueError('unexpected AUR package identity or architecture')
    versions = re.findall(r'^\s*pkgver = (.+)$', text, re.MULTILINE)
    if len(versions) != 1:
        raise ValueError('ambiguous AUR package version')
    return checked_version(versions[0])

incoming = files(recipes)
if pkg_version(incoming) != version:
    raise ValueError('recipe version does not match release')
if not any(path.name != '.git' for path in repository.iterdir()):
    print('update')
else:
    current = files(repository)
    old = tuple(map(int, pkg_version(current).split('.')))
    new = tuple(map(int, version.split('.')))
    if old > new:
        print('newer')
    elif old == new:
        if current != incoming:
            raise ValueError('same-version AUR recipes differ; manual review required')
        print('same')
    else:
        print('update')
PY
)"
if [[ "$state" != update ]]; then
    printf 'aur: %s package already present; nothing to push\n' "$state"
    exit 0
fi
if git -C "$work/aur" rev-parse --verify HEAD >/dev/null 2>&1; then
    [[ "$(git -C "$work/aur" symbolic-ref --short HEAD)" == master ]] || fail 'unexpected AUR default branch'
else
    git -C "$work/aur" symbolic-ref HEAD refs/heads/master
fi
mkdir "$work/gnupg"
export GNUPGHOME="$work/gnupg"
printf '%s\n' "$AUR_GPG_PRIVATE_KEY" | gpg --batch --import
records="$(gpg --batch --with-colons --list-secret-keys "$fingerprint")"
grep -Fq ":$fingerprint:" <<<"$records" || fail 'configured signing key was not imported'
printf '%s' "${AUR_GPG_PASSPHRASE:-}" >"$work/passphrase"
export AUR_PASSPHRASE_FILE="$work/passphrase"
cat >"$work/gpg-wrapper" <<'GPG'
#!/usr/bin/env bash
exec gpg --batch --pinentry-mode loopback --passphrase-file "$AUR_PASSPHRASE_FILE" "$@"
GPG
chmod 700 "$work/gpg-wrapper"
git -C "$work/aur" config user.name "$AUR_COMMIT_NAME"
git -C "$work/aur" config user.email "$AUR_COMMIT_EMAIL"
git -C "$work/aur" config gpg.format openpgp
git -C "$work/aur" config gpg.program "$work/gpg-wrapper"
git -C "$work/aur" config user.signingkey "$fingerprint"
git -C "$work/aur" config commit.gpgsign true
cp -- "$recipes/PKGBUILD" "$recipes/.SRCINFO" "$work/aur/"
git -C "$work/aur" add -- PKGBUILD .SRCINFO
git -C "$work/aur" commit -S -m "chore(release): update kodometer to $version"
git -C "$work/aur" verify-commit HEAD
# A concurrent update fails rather than force-pushing. Retrying rechecks its version.
git -C "$work/aur" push origin HEAD:master

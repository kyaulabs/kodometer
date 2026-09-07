# Native packages

Kodometer release packages target x86-64 only. AUR and RPM call this architecture `x86_64`; Debian packages call it `amd64`. There are no ARM builds.

## Supported distributions

| Target | Build environment | Release package |
| --- | --- | --- |
| Arch Linux | Rolling Arch, native `makepkg` | `kodometer-X.Y.Z-1-x86_64.pkg.tar.zst` and source-based AUR recipes |
| Ubuntu 26.04 LTS | Ubuntu 26.04 | `kodometer_X.Y.Z-1ubuntu26.04_amd64.deb` |
| Fedora 43 | Fedora 43 | `kodometer-X.Y.Z-1.fc43.x86_64.rpm` |
| Fedora 44 | Fedora 44 | `kodometer-X.Y.Z-1.fc44.x86_64.rpm` |

The minimum is Plasma 6.4. Ubuntu 24.04's standard desktop stack is too old; its LTS status does not make it compatible. The Ubuntu package is not a general Debian package, and Fedora packages are not RHEL packages.

`packaging/targets.json` defines the reviewed build matrix. The policy is the current compatible Ubuntu LTS and supported newer Ubuntu releases, plus supported stable Fedora releases with Plasma 6.4 or newer. Add releases after checking their development packages and passing native CI; remove expired targets. The workflow does not discover or enable untested distributions automatically.

The `v0.1.0` release remains archive-only. Native formats start with `v0.2.0`; existing public assets are not changed.

## Install or upgrade

Download the package and its `.sha256` file from the same [GitHub release](https://github.com/kyaulabs/kodometer/releases). Replace `X.Y.Z` with that release's version and choose the file for your distribution. For Fedora 43, use the `fc43` filename instead of `fc44`.

```bash
# Ubuntu 26.04
sha256sum -c kodometer_X.Y.Z-1ubuntu26.04_amd64.deb.sha256
sudo apt install ./kodometer_X.Y.Z-1ubuntu26.04_amd64.deb

# Fedora 44
sha256sum -c kodometer-X.Y.Z-1.fc44.x86_64.rpm.sha256
sudo dnf install ./kodometer-X.Y.Z-1.fc44.x86_64.rpm

# Arch Linux
sha256sum -c kodometer-X.Y.Z-1-x86_64.pkg.tar.zst.sha256
sudo pacman -U ./kodometer-X.Y.Z-1-x86_64.pkg.tar.zst
```

Log out of Plasma before replacing a loaded plugin; install from a TTY or SSH session, then log back in. Use the same installation method for upgrades. Remove manual system or user-local copies before switching to a package, so Qt cannot load an older duplicate.

Package managers resolve runtime dependencies. Ubuntu packages use native ELF dependency scanning; Fedora packages retain RPM's automatic runtime requirements. QML imports, Plasma, and the wallet service also have explicit dependencies. No Qt, KDE, or glibc libraries are bundled or copied from Arch into DEB/RPM files.

Uninstall with your package manager. Packages contain no OAuth files, wallet entries, or widget preferences, and have no scripts that delete them. There is no hosted APT/DNF repository or automatic desktop updater. DEB/RPM files and source tags are not independently signed by this workflow; checksums detect corruption, not publisher identity. Do not weaken your system's package-signature policy to install an untrusted file.

## AUR source package

Each release includes `kodometer-X.Y.Z-aur.tar.gz` with `PKGBUILD` and `.SRCINFO`, and `kodometer-X.Y.Z-source.tar.gz` with the exact build revision. The recipe downloads that fixed release source archive and checks its SHA-256 digest. It does not follow a Git branch or use `SKIP` checksums.

After verifying the AUR bundle's `.sha256` file, extract it into a new directory and review `PKGBUILD`. Run `makepkg -si` as a normal user on Arch. It builds the applet, runs the native tests, and installs the package. You can place the matching source archive beside `PKGBUILD` to reuse the download; `makepkg` still checks its digest.

Recipe generation does not require an AUR account. Publishing those recipes to `aur.archlinux.org/kodometer.git` does require package ownership and dedicated automation credentials. It is disabled until configured; see [AUR publication setup](releasing.md#aur-publication-setup). Do not assume the AUR entry exists merely because a release contains recipes.

## Build and verification pipeline

`.github/workflows/packages.yml` is shared by CI and releases. It checks out the requested revision and creates one source archive, then builds in separate Arch, Ubuntu, and Fedora containers. Release builds run the complete native test suite, including staged installation tests. Arch's `check()` runs those tests under an unprivileged `makepkg` user.

Separate runtime containers install and reinstall each package without development dependencies. An isolated, temporary Plasma session checks linkage, confirms the installed plugin is mapped into `plasmawindowed`, and rejects QML loading errors. It has a disabled wallet and no provider credentials. These checks do not modify the runner's desktop session.

The workflow collects seven payloads: source, AUR recipes, the Arch package, the legacy Arch-built tar archive, the Ubuntu DEB, and both Fedora RPMs. `scripts/package_metadata.py` requires every payload, creates individual checksums and `kodometer-X.Y.Z-SHA256SUMS`, and verifies the exact inventory. Missing files, symlinks, unknown assets, duplicate checksum rows, unsafe names, and mismatched digests fail the job. The `kodometer-release-bundle` CI artifact contains the complete release input.

The provisioning, build, and installation scripts are for disposable root build containers. Do not run them as installers on a working desktop. For a normal source installation, use the CMake commands in [README](../README.md#build-and-install).

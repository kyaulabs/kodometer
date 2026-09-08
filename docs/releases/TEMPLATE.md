# 🚀 Kodometer X.Y.Z

<!-- Copy to X.Y.Z.md and replace every placeholder. Keep all eight sections in
this order, even for patch releases: say "No changes" where appropriate.
The publisher appends the ninth section, "## 📜 Changelog", from git-cliff.
Update the package rows from packaging/targets.json and package_metadata.py;
do not assume the example matrix below will remain current forever. -->

## ✨ Highlights

Describe the release's purpose and the most important user-visible changes. State the previous version, the Conventional Commit evidence for this SemVer bump, and whether there are breaking changes or migrations. Do not infer the version from a branch name alone.

## 🚀 Features

Explain new behavior, where to find it, its defaults, and any opt-in settings. Identify unchanged behavior when that prevents confusion. For releases without features, say so rather than removing this section.

## 🐛 Fixes

Describe corrected symptoms and their practical effect. Separate fixes from internal maintenance and do not claim unsupported runtime validation. Say when no fixes are included.

## 📦 Packages

List every payload with its exact filename, distribution/version or source-build purpose, and architecture. These example rows must match the current inventory before publication.

| Distribution / purpose | Architecture | File |
| --- | --- | --- |
| Arch Linux (rolling), native package | x86_64 | `kodometer-X.Y.Z-1-x86_64.pkg.tar.zst` |
| Ubuntu 26.04 LTS, native package | amd64 | `kodometer_X.Y.Z-1ubuntu26.04_amd64.deb` |
| Fedora 43, native package | x86_64 | `kodometer-X.Y.Z-1.fc43.x86_64.rpm` |
| Fedora 44, native package | x86_64 | `kodometer-X.Y.Z-1.fc44.x86_64.rpm` |
| Arch Linux (rolling), legacy binary archive | x86_64 | `kodometer-X.Y.Z-linux-x86_64.tar.gz` |
| Arch Linux (rolling), AUR source recipes | x86_64 target | `kodometer-X.Y.Z-aur.tar.gz` |
| Source for local builds on compatible Linux | Released targets: x64 | `kodometer-X.Y.Z-source.tar.gz` |

Each payload has an individual `.sha256` sidecar. `kodometer-X.Y.Z-SHA256SUMS` lists the complete payload inventory. State whether AUR publication is actually configured/live; recipes alone are not evidence of an AUR entry. Distinguish native packages from the legacy Arch-built archive and GitHub's automatic source downloads.

## 🛠️ Installation and upgrade

Give version-specific checksum and native package-manager commands, with links to the tagged package guide. Explain the Plasma shutdown/restart requirement, system versus user-local installations, duplicate-plugin avoidance, and whether preferences or credentials need migration. Never present container provisioning helpers as desktop installers.

## ⚠️ Compatibility and known limitations

State minimum Plasma/Qt versions, supported architecture/distributions, unsupported targets, ABI limitations, and provider restrictions relevant to the release. Link to the tagged README for the full provider/authentication contract. Explicitly call out limitations that might be mistaken for new support.

## 🔒 Security and privacy

Describe relevant security changes and unchanged credential boundaries. Explain checksum versus signature guarantees, immutable published assets, and whether packaging touches credentials or settings. Do not include private account data, secrets, or real credential examples.

## ✅ Validation

Report performed checks and measured results, or describe publication gates clearly while preparation is still in progress. Cover native Debug/Release tests, the complete clean-runtime distro matrix, coverage gates, QML/format checks, Python automation tests, and artifact verification. Distinguish synthetic/offscreen checks from live desktop/provider validation. Never mark pending checks as completed.

# CodexBar Plasma

[![Conventional Commits](https://img.shields.io/badge/conventional%20commits-1.0.0-fe5196?logo=conventionalcommits)](https://www.conventionalcommits.org/en/v1.0.0/)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue)](LICENSE)

CodexBar Plasma is a KDE Plasma 6 panel widget for AI provider limits. It follows the layout and meter behavior of [CodexBar](https://github.com/steipete/CodexBar) while using Plasma controls, keyboard access, and theme colors.

The widget reads the stable dashboard-v1 JSON produced by the CodexBar CLI. Provider authentication, API access, local history scans, and provider configuration remain in the CLI. The widget does not read credential files or invoke a shell.

## Status

This project is under active development. The current foundation includes:

- a compiled Plasma 6 applet for Wayland and X11 sessions;
- session and weekly meters in the panel;
- provider tabs, an overview page, and detailed quota windows for every dashboard provider;
- reset countdowns, account and plan labels, credits, costs, and provider status details;
- a Qt/C++ process boundary with time and output limits;
- last-good data retention when a refresh fails;
- redacted account identity by default;
- C++ and QML tests, with line, function, and branch coverage gates above 95%;
- CI checks for formatting, builds, tests, QML, commits, dependencies, workflows, and leaked secrets.

Later feature branches will add settings, multi-account controls, cost history views, notifications, provider actions, and further visual parity work.

## Requirements

Runtime:

- KDE Plasma 6.0 or newer;
- Qt 6.4 or newer;
- CodexBar CLI with the `dashboard` command. Version 0.56.4 or newer is recommended.

Build:

- CMake 3.24 or newer;
- Ninja;
- a C++20 compiler;
- Extra CMake Modules;
- Qt 6 Core, QML, Quick Test, and development tools;
- KDE Frameworks 6 Config and CoreAddons;
- libplasma and Kirigami.

On Arch Linux, install the build dependencies with:

```bash
sudo pacman -S --needed base-devel cmake extra-cmake-modules \
  kconfig kcoreaddons kirigami libplasma ninja qt6-declarative
```

Install the CodexBar CLI separately, then check its dashboard output:

```bash
codexbar --version
codexbar dashboard --identity redacted --pretty
```

## Build and install

```bash
git clone https://github.com/kyaulabs/codexbar-plasma.git
cd codexbar-plasma
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME/.local"
kbuildsycoca6
```

Restart Plasma Shell after the first install, then add **CodexBar Plasma** from the widget browser. On a development machine, restart the shell with:

```bash
kquitapp6 plasmashell
kstart plasmashell
```

Release archives preserve system-relative paths. To install one for all users:

```bash
sha256sum -c codexbar-plasma-X.Y.Z-linux-x86_64.tar.gz.sha256
sudo tar -xzf codexbar-plasma-X.Y.Z-linux-x86_64.tar.gz -C /
```

## Development

Build and run all tests:

```bash
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target org.kyaulabs.codexbar_qmllint
ctest --test-dir build --output-on-failure
```

Check formatting and shell scripts:

```bash
scripts/check-format.sh
shellcheck scripts/*.sh
```

Generate the coverage report in `coverage/`:

```bash
scripts/coverage.sh
```

The coverage command fails below 96% for line, function, or branch coverage. QML visual primitives run through Qt Quick Test in an offscreen session.

Build the release archive and checksum:

```bash
scripts/package.sh
```

## Release flow

Development follows Git Flow and Conventional Commits.

1. Merge feature and fix branches into `develop`.
2. Create `release/X.Y.Z` from `develop` and update the versions in `CMakeLists.txt`, `applet/metadata.json`, and `package.json`.
3. Open the release pull request against `main`.
4. After that pull request merges, the release workflow validates the version, creates tag `vX.Y.Z`, packages the applet, and creates the GitHub release.
5. The workflow opens a `main` to `develop` back-merge pull request with the `KYAULABS_BOT_TOKEN` repository secret.

## Security and privacy

`DashboardController` starts the configured executable directly through `QProcess`. It does not concatenate commands or pass data through a shell. Dashboard identity is redacted unless the user explicitly changes that setting in a later configuration UI.

The CodexBar CLI owns provider credentials and network requests. Review its [privacy notes](https://github.com/steipete/CodexBar#privacy-note) before enabling browser cookies, local history, or process inspection.

Report vulnerabilities according to [SECURITY.md](SECURITY.md).

## Attribution

CodexBar Plasma is an independent Linux client inspired by Peter Steinberger's MIT-licensed [CodexBar](https://github.com/steipete/CodexBar). It consumes CodexBar's documented dashboard-v1 interface. CodexBar and its provider marks belong to their respective owners.

## License

CodexBar Plasma is licensed under the [GNU Affero General Public License v3.0](LICENSE).

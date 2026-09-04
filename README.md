# Kodometer

[![Conventional Commits](https://img.shields.io/badge/conventional%20commits-1.0.0-fe5196?logo=conventionalcommits)](https://www.conventionalcommits.org/en/v1.0.0/)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue)](LICENSE)

Kodometer is a native KDE Plasma 6 widget for monitoring AI-provider usage limits. It follows the compact meter model of [CodexBar](https://github.com/steipete/CodexBar) while using Qt, Plasma controls, keyboard navigation, and system theme colors.

Kodometer talks to provider APIs directly. It does not require CodexBar, invoke provider CLIs, inspect browser sessions, or pass commands through a shell.

## Status

Kodometer is under active development. The current release foundation includes:

- a compiled Plasma 6 applet for Wayland and X11;
- native Codex and Claude OAuth refresh and usage requests through Qt Network;
- secure loading and atomic rotation of provider credentials;
- session, weekly, model-specific, routines, and spend-limit windows;
- Claude monthly-cap and plan presentation;
- provider tabs, an overview, reset countdowns, and account and plan labels;
- last-good data retention when a refresh fails;
- redacted account identity by default;
- request timeouts, response-size limits, and manual redirect handling;
- C++ and QML tests with line, function, and branch coverage gates above 95%;
- CI checks for formatting, builds, tests, QML, commits, dependencies, workflows, and leaked secrets.

Codex and Claude are available as native providers. Planned adapters include Gemini, xAI, Kimi, DeepSeek, z.AI, and OpenRouter. KWallet-backed manual credential entry, settings, multi-account controls, cost history, notifications, and provider actions will follow in reviewable branches.

## Requirements

Runtime:

- KDE Plasma 6.0 or newer;
- Qt 6.4 or newer;
- a Codex login at `~/.codex/auth.json`, or under `$CODEX_HOME/auth.json`;
- a Claude login at `~/.claude/.credentials.json`.

Kodometer accepts the OAuth credentials written by Codex and Claude. Codex's `OPENAI_API_KEY` file form is also supported. Claude profile roots set through `CLAUDE_CONFIG_DIR` are honored, as is `CLAUDE_SECURESTORAGE_CONFIG_DIR`; relative profile paths resolve from Kodometer's working directory, matching Claude Code's literal-path behavior.

Credential files must be regular files owned by the current user. Kodometer rejects symbolic links, files larger than 1 MiB, and files that grant group or other users read or write access. To secure the default files:

```bash
chmod 600 "$HOME/.codex/auth.json" "$HOME/.claude/.credentials.json"
```

Build requirements:

- CMake 3.24 or newer;
- Ninja;
- a C++20 compiler;
- Extra CMake Modules;
- Qt 6 Core, Network, QML, Quick Test, and development tools;
- KDE Frameworks 6 Config and CoreAddons;
- libplasma and Kirigami.

On Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake extra-cmake-modules \
  kconfig kcoreaddons kirigami libplasma ninja qt6-declarative
```

## Build and install

```bash
git clone https://github.com/kyaulabs/kodometer.git
cd kodometer
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME/.local"
kbuildsycoca6
```

Restart Plasma Shell after the first install, then add **Kodometer** from the widget browser. On a development machine:

```bash
kquitapp6 plasmashell
kstart plasmashell
```

Release archives preserve system-relative paths. To install one for all users:

```bash
sha256sum -c kodometer-X.Y.Z-linux-x86_64.tar.gz.sha256
sudo tar -xzf kodometer-X.Y.Z-linux-x86_64.tar.gz -C /
```

## Development

Build and run all tests:

```bash
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target org.kyaulabs.kodometer_qmllint
ctest --test-dir build --output-on-failure
```

Run repository checks:

```bash
scripts/check-format.sh
shellcheck scripts/*.sh
scripts/coverage.sh
```

The coverage command writes reports to `coverage/` and fails below 96% for line, function, or branch coverage. QML visual primitives run through Qt Quick Test in an offscreen session.

Build the release archive and checksum:

```bash
scripts/package.sh
```

## Release flow

Development follows Git Flow and Conventional Commits.

1. Merge feature and fix branches into `develop`.
2. Create `release/X.Y.Z` from `develop` and update versions in `CMakeLists.txt`, `applet/metadata.json`, and `package.json`.
3. Open the release pull request against `main`.
4. After merge, the release workflow validates the version, creates tag `vX.Y.Z`, packages the applet, and creates the GitHub release.
5. The workflow opens a `main` to `develop` back-merge pull request with the `KYAULABS_BOT_TOKEN` repository secret.

## Security and privacy

Kodometer reads credentials only from the expected Codex and Claude authentication files. It rejects symbolic links, unexpected ownership, permissive file modes, non-regular files, and files larger than 1 MiB. OAuth refreshes are written with `QSaveFile` so replacement is atomic and permissions remain owner-only. Claude refresh-token rotation is persisted to the selected Claude credential file so later Claude Code and Kodometer sessions share the current token chain.

Network requests use fixed provider endpoints, bounded response buffers, explicit timeouts, and disabled automatic redirects. Account email addresses are redacted before data reaches QML. Tokens are never added to the presentation model or logs.

Report vulnerabilities according to [SECURITY.md](SECURITY.md).

## Attribution

Kodometer is an independent Linux client inspired by Peter Steinberger's MIT-licensed [CodexBar](https://github.com/steipete/CodexBar). CodexBar and provider names and marks belong to their respective owners.

## License

Kodometer is licensed under the [GNU Affero General Public License v3.0](LICENSE).

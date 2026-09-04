# Kodometer

[![Conventional Commits](https://img.shields.io/badge/conventional%20commits-1.0.0-fe5196?logo=conventionalcommits)](https://www.conventionalcommits.org/en/v1.0.0/)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue)](LICENSE)

Kodometer is a native KDE Plasma 6 widget for monitoring AI-provider usage limits. It follows the compact meter model of [CodexBar](https://github.com/steipete/CodexBar) while using Qt, Plasma controls, keyboard navigation, and system theme colors.

Kodometer talks to provider APIs directly. It does not require CodexBar, invoke provider CLIs, inspect browser sessions, or pass commands through a shell.

## Status

Kodometer is under active development. The current release foundation includes:

- a compiled Plasma 6 applet for Wayland and X11;
- native Codex, Claude, and Gemini OAuth requests plus xAI Management API billing through Qt Network;
- secure loading and atomic rotation of provider credentials;
- session, weekly, model-specific, routines, spend-limit, and Gemini tier windows;
- Claude monthly-cap and plan presentation;
- xAI prepaid balance and 30-day platform spend summaries;
- provider tabs, an overview, reset countdowns, and account and plan labels;
- last-good data retention when a refresh fails;
- redacted account identity by default;
- request timeouts, response-size limits, and manual redirect handling;
- C++ and QML tests with line, function, and branch coverage gates above 95%;
- CI checks for formatting, builds, tests, QML, commits, dependencies, workflows, and leaked secrets.

Codex, Claude, Gemini, and xAI are available as native providers. Planned adapters include Kimi, DeepSeek, z.AI, and OpenRouter. KWallet-backed manual credential entry, settings, multi-account controls, expanded cost history, notifications, and provider actions will follow in reviewable branches.

## Requirements

Runtime:

- KDE Plasma 6.0 or newer;
- Qt 6.4 or newer;
- a Codex login at `~/.codex/auth.json`, or under `$CODEX_HOME/auth.json`;
- a Claude login at `~/.claude/.credentials.json`;
- a Gemini CLI OAuth login at `~/.gemini/oauth_creds.json`;
- an xAI Management API key and team ID exported as `XAI_MANAGEMENT_API_KEY` and `XAI_TEAM_ID`.

Kodometer accepts OAuth credentials written by Codex, Claude, and Gemini CLI. Codex's `OPENAI_API_KEY` file form is also supported. Claude profile roots set through `CLAUDE_CONFIG_DIR` are honored, as is `CLAUDE_SECURESTORAGE_CONFIG_DIR`; relative profile paths resolve from Kodometer's working directory, matching Claude Code's literal-path behavior.

Gemini API-key and Vertex AI sessions do not expose the Code Assist OAuth quota endpoint and are not supported by this adapter. Token refresh reads Gemini CLI's public installed-app OAuth values from its installed JavaScript package without running the CLI. `GEMINI_OAUTH_CLIENT_ID` and `GEMINI_OAUTH_CLIENT_SECRET` override discovery; `GEMINI_OAUTH2_JS_PATH` selects a specific `oauth2.js` file. Following Google's June 2026 consumer-tier shutdown, Gemini quota access is limited to Workspace, education, and Code Assist Standard or Enterprise accounts. Individual, Google AI Pro, and Ultra accounts must use Antigravity instead.

xAI support targets developer-platform billing, not Grok or SuperGrok subscription quota. Create a Management API key with billing read access in the [xAI Console](https://console.x.ai), then export it with the team ID shown in the console URL:

```bash
export XAI_MANAGEMENT_API_KEY="..."
export XAI_TEAM_ID="team-id"
```

Kodometer requests the posted prepaid ledger balance and a best-effort 30-day daily USD spend series. A history failure does not hide a valid balance. The key remains in the process environment and is never written to disk; KWallet-backed entry will replace this environment-only setup in a later settings branch.

Credential files must be regular files owned by the current user. Kodometer rejects symbolic links, files larger than 1 MiB, and files that grant group or other users read or write access. To secure the default files:

```bash
chmod 600 "$HOME/.codex/auth.json" "$HOME/.claude/.credentials.json" \
  "$HOME/.gemini/oauth_creds.json"
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

Kodometer reads OAuth credentials only from the expected Codex, Claude, and Gemini authentication files. It rejects symbolic links, unexpected ownership, permissive file modes, non-regular files, and files larger than 1 MiB. OAuth refreshes are written with `QSaveFile` so replacement is atomic and permissions remain owner-only. Claude refresh-token rotation and Gemini access-token renewal are persisted to their provider-owned files so the provider tools and Kodometer share the current token state. The xAI Management API key is read from `XAI_MANAGEMENT_API_KEY` and is not persisted by Kodometer.

Network requests use fixed provider endpoints, bounded response buffers, explicit timeouts, and disabled automatic redirects. Account email addresses are redacted before data reaches QML. Tokens are never added to the presentation model or logs.

Report vulnerabilities according to [SECURITY.md](SECURITY.md).

## Attribution

Kodometer is an independent Linux client inspired by Peter Steinberger's MIT-licensed [CodexBar](https://github.com/steipete/CodexBar). CodexBar and provider names and marks belong to their respective owners.

## License

Kodometer is licensed under the [GNU Affero General Public License v3.0](LICENSE).

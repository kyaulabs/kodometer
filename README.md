# Kodometer

[![Conventional Commits](https://img.shields.io/badge/conventional%20commits-1.0.0-fe5196?logo=conventionalcommits)](https://www.conventionalcommits.org/en/v1.0.0/)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue)](LICENSE)

Kodometer is a native KDE Plasma 6 widget for monitoring AI-provider usage limits. It follows the compact meter model of [CodexBar](https://github.com/steipete/CodexBar) while using Qt, Plasma controls, keyboard navigation, and system theme colors.

Kodometer talks to provider APIs directly. It does not require CodexBar, invoke provider CLIs, inspect browser sessions, or pass commands through a shell.

## Status

Kodometer is under active development. The current release foundation includes:

- a compiled Plasma 6 applet for Wayland and X11;
- native Codex, Claude, DeepSeek, Gemini, Kimi Code, OpenRouter, xAI, and z.ai requests through Qt Network;
- secure loading, atomic OAuth rotation, and KWallet-backed API-key entry;
- session, weekly, model-specific, routines, spend-limit, and Gemini tier windows;
- Claude monthly-cap and plan presentation;
- xAI prepaid balance and 30-day platform spend summaries;
- Kimi Code 7-day and short-window request quotas;
- DeepSeek account balance with paid and granted credit breakdowns;
- z.ai and BigModel CN Coding Plan, MCP, model-token, and account-balance data;
- OpenRouter credit balance, API-key spending cap, and optional 30-day activity totals;
- 7-day and 30-day daily spend charts for xAI and OpenRouter;
- provider tabs, an overview, reset countdowns, and account and plan labels;
- official dashboard and documentation links from provider details;
- per-widget automatic refresh, provider switches, and idle-window preferences;
- named Codex and Claude credential profiles, with only the selected profile polled;
- named DeepSeek, Kimi Code, and OpenRouter API-key accounts in KWallet, with selected-only polling;
- opt-in low-quota desktop notifications with duplicate suppression;
- last-good data retention when a refresh fails;
- redacted account identity by default;
- request timeouts, response-size limits, and manual redirect handling;
- C++ and QML tests with line, function, and branch coverage gates above 95%;
- CI checks for formatting, builds, tests, QML, commits, dependencies, workflows, and leaked secrets.

Codex, Claude, DeepSeek, Gemini, Kimi Code, OpenRouter, xAI, and z.ai are available as native providers. Codex and Claude support named credential profiles; DeepSeek, Kimi Code, and OpenRouter support named KWallet accounts. Other providers still use one configured account.

## Requirements

Runtime:

- KDE Plasma 6.0 or newer;
- Qt 6.4 or newer, including Qt Quick Controls and Qt Quick Dialogs;
- KDE Frameworks 6 Wallet and Notifications, with a configured KDE Wallet service;
- a Codex login at `~/.codex/auth.json`, or under `$CODEX_HOME/auth.json`;
- a Claude login at `~/.claude/.credentials.json`;
- a Gemini CLI OAuth login at `~/.gemini/oauth_creds.json`;
- a Kimi Code API key exported as `KIMI_CODE_API_KEY`, or a fresh Kimi Code CLI login at `~/.kimi-code/credentials/kimi-code.json`;
- an xAI Management API key and team ID exported as `XAI_MANAGEMENT_API_KEY` and `XAI_TEAM_ID`;
- a DeepSeek API key exported as `DEEPSEEK_API_KEY`;
- a z.ai API key exported as `Z_AI_API_KEY`;
- an OpenRouter API key exported as `OPENROUTER_API_KEY`.

Open the widget's **Configure Kodometer…** action to store DeepSeek, Kimi Code, OpenRouter, xAI, or z.ai API keys in KDE Wallet. Kodometer stores entries in a `Kodometer` folder of the network wallet and never copies wallet values into Plasma configuration. In Default mode, a non-empty environment credential takes precedence over its matching wallet entry. Named DeepSeek, Kimi Code, and OpenRouter accounts instead use only their selected wallet keys. OpenRouter's ordinary and Management keys are separate entries; `XAI_TEAM_ID` and z.ai region, scope, organization, and project selectors remain environment settings.

Kodometer accepts OAuth credentials written by Codex, Claude, and Gemini CLI. Codex's `OPENAI_API_KEY` file form is also supported. Claude profile roots set through `CLAUDE_CONFIG_DIR` are honored, as is `CLAUDE_SECURESTORAGE_CONFIG_DIR`; relative profile paths resolve from Kodometer's working directory, matching Claude Code's literal-path behavior.

Gemini API-key and Vertex AI sessions do not expose the Code Assist OAuth quota endpoint and are not supported by this adapter. Token refresh reads Gemini CLI's public installed-app OAuth values from its installed JavaScript package without running the CLI. `GEMINI_OAUTH_CLIENT_ID` and `GEMINI_OAUTH_CLIENT_SECRET` override discovery; `GEMINI_OAUTH2_JS_PATH` selects a specific `oauth2.js` file. Following Google's June 2026 consumer-tier shutdown, Gemini quota access is limited to Workspace, education, and Code Assist Standard or Enterprise accounts. Individual, Google AI Pro, and Ultra accounts must use Antigravity instead.

xAI support targets developer-platform billing, not Grok or SuperGrok subscription quota. Create a Management API key with billing read access in the [xAI Console](https://console.x.ai), then export it with the team ID shown in the console URL:

```bash
export XAI_MANAGEMENT_API_KEY="..."
export XAI_TEAM_ID="team-id"
```

Kodometer requests the posted prepaid ledger balance and a best-effort 30-day daily USD spend series. A history failure does not hide a valid balance. The Management API key can instead be stored through Kodometer's credential settings. The team ID remains in the process environment.

Kimi support targets [Kimi For Coding](https://www.kimi.com/code), not the separate Moonshot/Kimi Open Platform. Export `KIMI_CODE_API_KEY` for the recommended API-key flow. Without that variable, Kodometer reuses a fresh access token from the official Kimi Code CLI and sends the CLI device identity headers. `KIMI_CODE_HOME` selects a non-default CLI home. Kodometer does not use the stored refresh token or rewrite the credential file; an expired login must be renewed with Kimi Code CLI. In Default mode, if an API key is rejected and a fresh CLI login exists, Kodometer retries once with the CLI credential. Named KWallet accounts never use this fallback. Browser cookies and `KIMI_AUTH_TOKEN` are not used.

DeepSeek support uses the documented account-balance endpoint. Create an API key in the [DeepSeek Platform](https://platform.deepseek.com), then export it before starting Plasma:

```bash
export DEEPSEEK_API_KEY="..."
```

`DEEPSEEK_KEY` is accepted as a compatibility alias. Kodometer shows the funded currency's total, paid, and granted balances, preferring a funded USD row when the API returns more than one currency. It does not inspect DeepSeek browser sessions or call private dashboard usage and cost endpoints. The key can instead be stored through Kodometer's credential settings in KDE Wallet.

z.ai support defaults to the global Coding Plan API. Export the API key before starting Plasma:

```bash
export Z_AI_API_KEY="..."
```

For a China-mainland account, set `Z_AI_REGION=bigmodel-cn`. That region also accepts `BIGMODEL_API_KEY`, `ZHIPU_API_KEY`, `ZHIPUAI_API_KEY`, or `GLM_API_KEY`. If those variables are absent, Kodometer securely checks `~/.config/bigmodel/api_key` and `~/.config/zhipu/api_key` in that order.

BigModel team usage requires three additional values:

```bash
export Z_AI_USAGE_SCOPE="team"
export Z_AI_BIGMODEL_ORGANIZATION="org-id"
export Z_AI_BIGMODEL_PROJECT="project-id"
```

Kodometer reads Coding Plan and MCP limits first. Hourly and daily model-token summaries are best effort, as is the BigModel CN account balance; failures in those optional requests do not hide valid quota data. Requests use fixed regional endpoints. Kodometer does not inspect browser cookies or accept endpoint overrides from the environment.

OpenRouter support uses the documented credits and key endpoints. Export a standard API key before starting Plasma:

```bash
export OPENROUTER_API_KEY="sk-or-v1-..."
```

Kodometer shows purchased credits, total account usage, remaining balance, API-key spend, and any configured key limit. `OPENROUTER_HTTP_REFERER` and `OPENROUTER_X_TITLE` set the optional application-identification headers. Key metadata is best effort and has a one-second deadline, so a slow or malformed response does not hide a valid credit balance.

For exact spend across the last 30 completed UTC days, export a management credential with Activity read access:

```bash
export OPENROUTER_MANAGEMENT_API_KEY="..."
```

Activity requests always use OpenRouter's production endpoint. Their failures remain visible as diagnostics without discarding credits or key-limit data. Kodometer ignores endpoint overrides, browser sessions, and OpenRouter website cookies. Either key can instead be stored through Kodometer's credential settings in KDE Wallet.

Credential files must be regular files owned by the current user. Kodometer rejects symbolic links, files larger than 1 MiB, and files that grant group or other users read or write access. To secure the default files:

```bash
chmod 600 "$HOME/.codex/auth.json" "$HOME/.claude/.credentials.json" \
  "$HOME/.gemini/oauth_creds.json" \
  "$HOME/.kimi-code/credentials/kimi-code.json" \
  "$HOME/.config/bigmodel/api_key" "$HOME/.config/zhipu/api_key"
```

Build requirements:

- CMake 3.24 or newer;
- Ninja;
- a C++20 compiler;
- Extra CMake Modules;
- Qt 6 Core, Gui, Network, QML, Quick Test, and development tools;
- KDE Frameworks 6 Config, CoreAddons, Notifications, and Wallet;
- `dbus-run-session` for isolated notification integration tests;
- libplasma and Kirigami.

On Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake extra-cmake-modules \
  dbus kconfig kcoreaddons kirigami knotifications kwallet libplasma ninja qt6-declarative
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

## Widget settings

Open **Configure Kodometer… → General** to choose which providers run and how often they refresh.

- **Refresh automatically** is on by default. Kodometer waits five minutes after each completed cycle, including failed cycles, before starting another. Choose an interval from 1 to 1440 minutes. Polling continues while the popup is closed, but requests never overlap.
- Turn automatic refresh off for startup and manual refresh only. Credential changes and provider switches still trigger a refresh; changes during an active cycle queue one follow-up cycle.
- Uncheck a provider to hide its data and errors and skip it on future refreshes. Requests already in progress may finish. KWallet entries and last-good data are retained so the provider can be re-enabled. Disabling all providers stops polling.
- **Show idle quota windows** is off by default. Enable it to include windows marked idle in the overview, provider details, and account summaries.

These preferences use Plasma's per-widget configuration and its Apply, Cancel, and Defaults controls. The separate **Credentials** page writes directly to KWallet when you press Save, Replace, or Remove; Cancel does not undo wallet changes. OAuth credentials and API keys are never stored in the general settings file.

## Codex and Claude profiles

Open **Configure Kodometer… → OAuth profiles** to add an existing credential folder and a nonsecret name, such as Work. Codex folders must contain `auth.json`; Claude folders must contain `.credentials.json`. Use **Browse…** or enter an absolute path, then **Add and select** and **Apply**. Kodometer does not sign in, copy credentials, or change the CLI's active login. Codex's existing API-key file form remains supported.

Each provider accepts up to eight named profiles plus **Default (environment)**. Default uses the existing environment overrides and standard credential locations. Named profiles override that provider's folder discovery. Names must be unique within a provider and at most 64 characters; paths are limited to 4096 characters. Duplicate named folder paths are rejected after path normalization. Names, paths, generated profile IDs, and selections are stored in per-widget configuration—not tokens or credential contents. Do not put secrets in profile names.

Only the selected profile is refreshed. Applying a selection change clears that provider's displayed data and errors and queues a normal refresh, even with automatic refresh disabled. Existing requests may finish against their original credential file, but their results cannot populate the new selection. Last-good data remains available after failures within the same profile; it is not reused across profile changes, including when switching back. Other enabled providers continue normally.

The page follows Plasma's Apply, Cancel, and Defaults controls. Removing an active profile selects Default without deleting any credential files. Invalid stored profile configuration pauses Codex and Claude rather than silently using another account; restore Defaults to recover. File ownership, permissions, size, and type are still checked before credentials are used. Normal OAuth renewal may update the selected credential file atomically.

## Named KWallet accounts

Open **Configure Kodometer… → KWallet accounts** to add a DeepSeek, Kimi Code, or OpenRouter account name and API key, then choose **Add account** and **Apply**. Each provider supports up to eight named accounts. Names must be unique within the provider and contain 1–64 characters. Keys must be nonempty printable ASCII without internal whitespace and at most 64 KiB. Do not put secrets in names.

OpenRouter accounts require an ordinary API key and accept an optional Management key from the same account for Activity. Both keys are saved together. **Replace selected keys** requires re-entering the ordinary key and the desired Management key; leaving Management blank removes it and disables Activity. Kodometer never borrows a missing named Management key from Default or the environment. Activity failures still preserve valid credits. Optional HTTP referer and client-title headers remain application-level environment settings.

Named accounts use only the selected KWallet keys. They ignore environment keys, DeepSeek's compatibility alias, and Kimi's CLI credentials. **Default** restores the existing environment/wallet precedence and Kimi CLI discovery. Only the selected account is polled; applying a selection clears that provider's old data and queues a normal refresh, even with automatic refresh disabled.

Account names and keys are stored together in KWallet and shared by widgets using that wallet. Saves, key replacements, and removals take effect immediately; Cancel does not undo them. Each widget stores only its selected account UUIDs in Plasma configuration. Selections follow Apply/Cancel/Defaults, and Defaults does not delete wallet entries. The entry fields never reveal saved keys.

A locked or unreadable wallet, malformed account data, or a removed selection pauses providers using named accounts rather than silently choosing another credential. Existing requests may finish, but results from changed keys or selections are discarded. Unlock the wallet and press the widget's **Refresh** button to reconnect; use **Open / retry KWallet** in settings to reload its account list. Repair malformed named entries with KWallet Manager. Removing an account leaves affected widgets paused until another account or Default is explicitly selected. Label-only edits and changes to unselected accounts do not refetch; replacing a selected key clears its retained data.

## Cost history

Provider details show **Daily spend (USD)** when xAI or OpenRouter returns daily history. Choose 7 or 30 days; the range selector changes only the view and does not fetch data or persist a preference. Click a bar, or focus it with Tab and press Space, to read that day's amount. Hover and keyboard focus also expose date-and-amount tooltips.

The chart uses UTC calendar days anchored to the snapshot. xAI includes the incomplete day when the refresh started; OpenRouter ends on the preceding completed day. Refreshes keep one date window even if their requests cross midnight. Retained snapshots keep their original dates rather than sliding forward with the clock.

Only explicitly reported daily amounts contribute to the chart's total. Missing days say **Not reported**, not `$0.00`; a positive amount below one cent says **<$0.01**. Partial history, unreported days, incomplete days, and provider estimates have visible notes. Provider-level partial and estimated flags apply to both ranges. Daily sums can differ from other billing figures, which may cover different periods or scopes.

The chart uses existing adapter results—no extra requests, credential access, billing-data files, or Qt Charts dependency. Invalid history hides the chart without hiding valid balances. Providers that expose only balances or quota do not get a synthetic spend history.

## Quota notifications

Notifications are off by default. Enable **Notify when quota is low** in General settings and choose a threshold from 1% to 50% remaining; the default is 10%.

A fresh successful result at or below the threshold produces one alert for that provider. Kodometer uses its most constrained non-idle quota window, including reported spending caps. Balance-only data, failed refreshes, retained snapshots, and invalid percentages do not generate alerts. Invalid active windows also cannot prove recovery.

A provider is rearmed after its valid active quotas recover to at least five percentage points above the threshold. At the default threshold, quota must recover to 15% before another drop to 10% can alert. Results with no usable quota do not rearm the provider. This state is per provider, per widget session; restarting the widget, toggling notifications, or changing the threshold clears it. Settings changes wait for a fresh result rather than replaying cached data. Changing an OAuth profile or selected KWallet account, replacing its key, or losing access to it clears suppression only for that provider. A fresh low result can alert again, including after switching back or unlocking the wallet. Profile names, account names, and paths never enter notification text.

Alerts contain only a public provider name and the remaining percentage—not account identities, window labels, or credentials. KDE Notifications delivers normal-urgency popups under your desktop notification and Do Not Disturb settings. Delivery is best effort; Kodometer does not retry suppressed or undelivered alerts. The default event has no sound or actions.

## Provider actions

Provider details include **Open dashboard** and **Documentation** buttons. They open official pages in your default browser only when clicked. Hover over a button to preview its destination. If the desktop cannot launch the page, Kodometer shows an error without changing the usage snapshot.

Destinations are fixed in the C++ action catalog. Provider response URLs, account IDs, API keys, and OAuth tokens never enter the links. z.ai uses its global or BigModel CN destinations according to the normalized region of the displayed snapshot; unknown regions have no actions. Gemini opens Google Cloud Console rather than the unsupported consumer Gemini dashboard. BigModel CN opens the console home so you can choose the appropriate personal or team view.

Your browser may ask you to sign in. Kodometer does not read the browser session or import credentials from it. These links do not change provider authentication or refresh behavior.

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

The coverage command scans only its freshly instrumented build directory, writes reports to `coverage/`, and fails below 96% for line, function, or branch coverage. QML primitives and general settings controls run through Qt Quick Test in an offscreen session. Applet-enabled builds also test the compiled configuration resources, KConfig persistence, and idle-window presentation without opening a wallet or calling provider APIs. Notification delivery tests run against a fake service on a private D-Bus session, never the desktop's notification service.

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

Kodometer reads OAuth credentials only from the expected Codex, Claude, Gemini, and Kimi Code authentication files. It rejects symbolic links, unexpected ownership, permissive file modes, non-regular files, and files larger than 1 MiB. OAuth refreshes are written with `QSaveFile` so replacement is atomic and permissions remain owner-only. Claude refresh-token rotation and Gemini access-token renewal are persisted to their provider-owned files so the provider tools and Kodometer share the current token state. Kimi Code credentials remain read-only; Kodometer creates only a missing owner-only device ID required by the official API. Manually entered API keys are held by KDE Wallet, not plaintext Plasma configuration. Non-empty API-key environment values retain precedence over corresponding Default KWallet entries. Explicit named DeepSeek, Kimi Code, and OpenRouter accounts bypass that discovery and have no credential fallback. BigModel CN and Zhipu key files are read-only inputs.

Network requests use fixed provider endpoints, bounded response buffers, explicit timeouts, and disabled automatic redirects. Account email addresses are redacted before data reaches QML. Tokens are never added to the presentation model or logs.

Report vulnerabilities according to [SECURITY.md](SECURITY.md).

## Attribution

Kodometer is an independent Linux client inspired by Peter Steinberger's MIT-licensed [CodexBar](https://github.com/steipete/CodexBar). CodexBar and provider names and marks belong to their respective owners.

## License

Kodometer is licensed under the [GNU Affero General Public License v3.0](LICENSE).

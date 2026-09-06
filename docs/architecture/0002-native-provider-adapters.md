# ADR 0002: Native provider adapters

- Status: Accepted
- Date: 2026-09-03
- Supersedes: [ADR 0001](0001-native-plasma-client.md)

## Context

The original applet delegated authentication and usage collection to the CodexBar CLI. That kept the first implementation small, but made an unrelated executable a runtime dependency and constrained Linux integration to the CLI's provider contract.

Kodometer needs provider support that is native to Linux, independently testable, and suitable for Plasma settings. Provider credentials also require stricter boundaries than presentation data: existing tool credentials should be reused without copying secrets, while manually entered values should live in the desktop secret store.

## Decision

Implement each provider as a C++ `ProviderAdapter`. Adapters authenticate, make bounded asynchronous requests through Qt Network, and normalize results into the existing provider presentation map. `UsageController` refreshes registered adapters, preserves each adapter's last successful result, and publishes providers in registration order.

Do not invoke provider CLIs or retain a subprocess fallback. Existing provider credential files are accepted only after provider-specific ownership, type, size, and permission checks. Manually entered API keys are stored in a dedicated folder of KDE's network wallet rather than plaintext configuration files. Wallet opening is asynchronous, values are validated before storage, and QML receives configured-key names and named-account metadata, never saved secret values. Non-empty process-environment credentials retain precedence in Default mode.

The Codex adapter reads `auth.json` from `$CODEX_HOME`, or `~/.codex` by default; extracts identity and expiry claims from OAuth tokens; refreshes expiring tokens; persists rotations atomically with owner-only permissions; and maps usage windows directly into the applet schema. Account identity is redacted before publication to QML.

The Claude adapter reads the selected Claude Code `.credentials.json`, including `CLAUDE_CONFIG_DIR` and `CLAUDE_SECURESTORAGE_CONFIG_DIR` profile boundaries. It refreshes expiring credentials through Anthropic's OAuth token endpoint, atomically updates the shared credential file, and maps session, weekly, model-scoped, routines, monthly-cap, and spend-limit data. No Claude executable or browser session is used.

The Gemini adapter reads Gemini CLI's `oauth_creds.json` and authentication selection. It resolves the CLI's public installed-app OAuth client from environment overrides or installed JavaScript, without starting Gemini or another process. The adapter renews and atomically persists access tokens, loads Code Assist tier and project metadata, optionally discovers a suitable Cloud project, and maps the most constrained Pro, Flash, and Flash Lite quota buckets. It identifies Google's June 2026 consumer-tier shutdown while leaving Workspace, education, and licensed Code Assist accounts enabled.

The xAI adapter uses a Management API key from KWallet or the process environment and an explicit team ID from the process environment. It reads the team's posted prepaid balance and requests a best-effort 30-day daily spend series. Billing authentication failures invalidate the refresh, while analytics, parse, network, timeout, and size failures preserve a valid balance. The adapter does not treat prepaid balance as spend or share credentials with the separate Grok consumer service.

The Kimi adapter targets Kimi For Coding rather than the Moonshot/Kimi Open Platform. In Default mode it prefers `KIMI_CODE_API_KEY`, then securely reads a fresh access token and stable device identity from the official Kimi Code CLI home. CLI credentials remain read-only because Kodometer neither invokes the CLI nor uses its refresh token. A rejected Default key receives one retry with a fresh CLI credential. Named KWallet accounts disable CLI fallback for the entire request, even if selection changes while it is pending. The adapter maps the Code API's 7-day request allowance and first short-window rate limit; it does not inspect browser cookies or accept `KIMI_AUTH_TOKEN`.

The DeepSeek adapter reads `DEEPSEEK_API_KEY`, with `DEEPSEEK_KEY` as a compatibility alias, and calls the documented `/user/balance` endpoint. It maps total, paid, and granted credits in the currency reported by DeepSeek, preferring a funded USD row without hiding a funded non-USD row behind an empty USD balance. Browser sessions and private dashboard usage or cost endpoints are outside the security boundary and are not used.

The z.ai adapter supports the global and BigModel CN Coding Plan APIs with explicit regional credential boundaries. China-mainland aliases and provider key files are ignored for the global route. BigModel team requests require organization and project selectors. Quota is authoritative; hourly and daily model-token requests and the China account-balance request are best effort. Production routing is fixed to the selected provider hosts, and browser cookies or environment endpoint overrides are not accepted.

The OpenRouter adapter reads `OPENROUTER_API_KEY` and calculates prepaid balance from the documented credits endpoint. Key metadata is optional enrichment: a failure or one-second timeout preserves credits while recording a diagnostic. Configured key limits are treated as spending caps rather than account balances. `OPENROUTER_MANAGEMENT_API_KEY` enables best-effort Activity requests for spend and token totals across the last 30 completed UTC days. Management credentials use a fixed production endpoint and are never sent to an override or reused for ordinary API calls.

Daily cost presentation uses the existing `cost.daily` USD series plus a normalized `historyEndDate`, `historyIncludesCurrentDay`, and optional `historyEstimated` flag. xAI and OpenRouter capture one UTC timestamp at refresh start for request windows, Activity validation, and snapshot publication. xAI clips history to its requested 30-day window, rejects overflowing spend aggregates, and omits today's spend if that day was not reported.

`CostHistoryModel` validates at most 366 input rows and emits at most 30 calendar-day bars. It rejects malformed dates, nonnumeric or negative amounts, duplicates, unsupported currencies, and overflowing selected totals. Missing days remain unknown rather than becoming zero. Provider partial/estimated flags are preserved across 7-day and 30-day views. The Qt Quick chart has keyboard selection and accessible date-and-amount labels; it adds no networking, credential access, local history storage, or chart-library dependency.

Nonsecret widget preferences use a KConfig schema embedded in the compiled applet. `UsageController` owns a single-shot refresh timer, bounded to 1–1440 minutes and armed only after a cycle completes. Disabled providers are excluded from future cycles, presentation, and error summaries. Existing requests may finish, but their results remain hidden while the provider is disabled. A pending-adapter set rejects duplicate or unsolicited completion signals. Changing provider enablement during a cycle queues one follow-up refresh.

Codex and Claude support selected-only credential profiles. `OAuthProfiles` validates a versioned, 64 KiB metadata document containing at most eight named profiles per provider, absolute folder paths, opaque UUIDs, and selections. It performs no credential-file I/O. Unknown fields, invalid paths/IDs, duplicate names/folders, and unresolved selections are rejected. Invalid persisted metadata blocks these two providers rather than falling back to Default. The configuration page stages edits under Plasma's Apply/Cancel semantics; removal changes metadata only.

`UsageController` owns the profile model and tracks a revision per provider. A context change atomically clears affected snapshots and errors, resets only that provider's notification suppression, and queues a coalesced refresh after any active cycle. Results from older revisions are discarded, even across A → B → A switches. Presentation callbacks are rechecked before publishing fresh-result signals. Profile-label changes alone do not refetch. Adapters apply folder selections only before a new cycle and pin credential reads and OAuth rotation to the request's original file, independently of later path setters. No CLI login selection or credential copying occurs.

DeepSeek and Kimi Code also support selected-only KWallet accounts. `WalletAccounts`, owned by `CredentialStore`, loads up to eight named entries per provider. Each password entry is `accounts/<provider>/<canonical UUID>` in the `Kodometer` folder and contains a JSON object with `name` and `key` fields. Names and secrets are written together, without a separate shared index. The backend rejects folders with more than 64 entries; parsed named records are bounded to 256 KiB each, labels to 64 characters, and printable ASCII keys to 64 KiB. Invalid fields, IDs, duplicate names, or oversized registries fail closed without disabling Default credential entries. Read revisions prevent reentrant wallet-close or update signals from restoring stale credentials.

Plasma stores only `deepseekAccountId` and `kimiAccountId` for these selections; empty values mean Default. Account mutations are immediate and shared across widgets, while selections use Apply/Cancel. Named requests ignore environment keys and CLI credentials. Missing selections and unavailable wallets pause the affected providers. `UsageController` extends its context revisions with internal key fingerprints, clearing old snapshots and notification suppression after selection, key, or availability changes. Neither fingerprints nor keys enter presentation data. Unselected edits and label-only changes do not refetch. Manual widget refresh also retries wallet access; automatic polling does not reopen a locked wallet.

Provider dashboard and documentation actions are separate from data collection. `ProviderActions` resolves provider IDs, action IDs, and normalized z.ai regions against a compiled HTTPS destination catalog. QML cannot pass an arbitrary URL to the opener. Explicit user clicks use `QDesktopServices`; merely loading or refreshing provider data never launches a page. URLs contain no credentials or account selectors, and browser sessions remain outside Kodometer's data sources.

Low-quota notifications are opt-in. `UsageController::providerRefreshed` emits only fresh successful results from enabled, pending adapters; retained snapshots and failed requests cannot trigger it. `QuotaAlertPolicy` evaluates finite numeric remaining percentages from active windows and tracks one low-quota episode per known provider. Recovery requires a five-percentage-point margin above the configured threshold. Invalid active windows cannot rearm an episode. State is bounded to the eight provider IDs and lasts for the widget session, not across restarts or notification-setting changes.

`QuotaNotifier` delivers policy events through KDE Notifications at normal urgency. Notifications contain fixed public provider names and percentages, never provider-supplied labels, identities, or credentials. The default event requests only a popup; desktop suppression and delivery failures do not cause retries. A private D-Bus fake tests the native transport without sending desktop notifications.

All network adapters must use fixed HTTPS endpoints in production, disable automatic redirects, impose request timeouts and response-size limits, avoid logging credentials, and test success, authentication failure or renewal where applicable, malformed data, network failure, timeout, and oversized-response paths against local servers.

## Consequences

- Kodometer has no CodexBar runtime dependency.
- Provider behavior and release cadence are controlled by this project.
- Each provider requires dedicated authentication, parsing, security review, and maintenance.
- Credential-file compatibility can change when upstream tools change their formats.
- Native networking keeps refreshes asynchronous and works equally under Wayland and X11.
- KWallet is a runtime and build dependency for the compiled applet.
- Credential changes reload adapters and queue one follow-up refresh when a request is already active.
- New adapters can reuse the normalized provider map and extend generic presentation fields when a provider exposes a new billing shape.

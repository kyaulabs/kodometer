# ADR 0002: Native provider adapters

- Status: Accepted
- Date: 2026-09-03
- Supersedes: [ADR 0001](0001-native-plasma-client.md)

## Context

The original applet delegated authentication and usage collection to the CodexBar CLI. That kept the first implementation small, but made an unrelated executable a runtime dependency and constrained Linux integration to the CLI's provider contract.

Kodometer needs provider support that is native to Linux, independently testable, and suitable for Plasma settings. Provider credentials also require stricter boundaries than presentation data: existing tool credentials should be reused without copying secrets, while manually entered values should live in the desktop secret store.

## Decision

Implement each provider as a C++ `ProviderAdapter`. Adapters authenticate, make bounded asynchronous requests through Qt Network, and normalize results into the existing provider presentation map. `UsageController` refreshes registered adapters, preserves each adapter's last successful result, and publishes providers in registration order.

Do not invoke provider CLIs or retain a subprocess fallback. Existing provider credential files are accepted only after provider-specific ownership, type, size, and permission checks. Manually entered API keys are stored in a dedicated folder of KDE's network wallet rather than plaintext configuration files. Wallet opening is asynchronous, values are validated before storage, and QML receives only configured-key names—not secret values. Non-empty process-environment credentials retain precedence.

The Codex adapter reads `auth.json` from `$CODEX_HOME`, or `~/.codex` by default; extracts identity and expiry claims from OAuth tokens; refreshes expiring tokens; persists rotations atomically with owner-only permissions; and maps usage windows directly into the applet schema. Account identity is redacted before publication to QML.

The Claude adapter reads the selected Claude Code `.credentials.json`, including `CLAUDE_CONFIG_DIR` and `CLAUDE_SECURESTORAGE_CONFIG_DIR` profile boundaries. It refreshes expiring credentials through Anthropic's OAuth token endpoint, atomically updates the shared credential file, and maps session, weekly, model-scoped, routines, monthly-cap, and spend-limit data. No Claude executable or browser session is used.

The Gemini adapter reads Gemini CLI's `oauth_creds.json` and authentication selection. It resolves the CLI's public installed-app OAuth client from environment overrides or installed JavaScript, without starting Gemini or another process. The adapter renews and atomically persists access tokens, loads Code Assist tier and project metadata, optionally discovers a suitable Cloud project, and maps the most constrained Pro, Flash, and Flash Lite quota buckets. It identifies Google's June 2026 consumer-tier shutdown while leaving Workspace, education, and licensed Code Assist accounts enabled.

The xAI adapter uses a Management API key from KWallet or the process environment and an explicit team ID from the process environment. It reads the team's posted prepaid balance and requests a best-effort 30-day daily spend series. Billing authentication failures invalidate the refresh, while analytics, parse, network, timeout, and size failures preserve a valid balance. The adapter does not treat prepaid balance as spend or share credentials with the separate Grok consumer service.

The Kimi adapter targets Kimi For Coding rather than the Moonshot/Kimi Open Platform. It prefers `KIMI_CODE_API_KEY`, then securely reads a fresh access token and stable device identity from the official Kimi Code CLI home. CLI credentials remain read-only because Kodometer neither invokes the CLI nor uses its refresh token. A rejected explicit key receives one retry with a fresh CLI credential. The adapter maps the Code API's 7-day request allowance and first short-window rate limit; it does not inspect browser cookies or accept `KIMI_AUTH_TOKEN`.

The DeepSeek adapter reads `DEEPSEEK_API_KEY`, with `DEEPSEEK_KEY` as a compatibility alias, and calls the documented `/user/balance` endpoint. It maps total, paid, and granted credits in the currency reported by DeepSeek, preferring a funded USD row without hiding a funded non-USD row behind an empty USD balance. Browser sessions and private dashboard usage or cost endpoints are outside the security boundary and are not used.

The z.ai adapter supports the global and BigModel CN Coding Plan APIs with explicit regional credential boundaries. China-mainland aliases and provider key files are ignored for the global route. BigModel team requests require organization and project selectors. Quota is authoritative; hourly and daily model-token requests and the China account-balance request are best effort. Production routing is fixed to the selected provider hosts, and browser cookies or environment endpoint overrides are not accepted.

The OpenRouter adapter reads `OPENROUTER_API_KEY` and calculates prepaid balance from the documented credits endpoint. Key metadata is optional enrichment: a failure or one-second timeout preserves credits while recording a diagnostic. Configured key limits are treated as spending caps rather than account balances. `OPENROUTER_MANAGEMENT_API_KEY` enables best-effort Activity requests for spend and token totals across the last 30 completed UTC days. Management credentials use a fixed production endpoint and are never sent to an override or reused for ordinary API calls.

Nonsecret widget preferences use a KConfig schema embedded in the compiled applet. `UsageController` owns a single-shot refresh timer, bounded to 1–1440 minutes and armed only after a cycle completes. Disabled providers are excluded from future cycles, presentation, and error summaries. Existing requests may finish, but their results remain hidden while the provider is disabled. A pending-adapter set rejects duplicate or unsolicited completion signals. Changing provider enablement during a cycle queues one follow-up refresh.

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

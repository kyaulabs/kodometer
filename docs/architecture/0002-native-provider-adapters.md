# ADR 0002: Native provider adapters

- Status: Accepted
- Date: 2026-09-03
- Supersedes: [ADR 0001](0001-native-plasma-client.md)

## Context

The original applet delegated authentication and usage collection to the CodexBar CLI. That kept the first implementation small, but made an unrelated executable a runtime dependency and constrained Linux integration to the CLI's provider contract.

Kodometer needs provider support that is native to Linux, independently testable, and suitable for Plasma settings. Provider credentials also require stricter boundaries than presentation data: existing tool credentials should be reused without copying secrets, while manually entered values should live in the desktop secret store.

## Decision

Implement each provider as a C++ `ProviderAdapter`. Adapters authenticate, make bounded asynchronous requests through Qt Network, and normalize results into the existing provider presentation map. `UsageController` refreshes registered adapters, preserves each adapter's last successful result, and publishes providers in registration order.

Do not invoke provider CLIs or retain a subprocess fallback. Existing provider credential files are accepted only after provider-specific ownership, type, size, and permission checks. Future manually entered secrets will be stored through KWallet rather than plaintext configuration files.

The Codex adapter reads `auth.json` from `$CODEX_HOME`, or `~/.codex` by default; extracts identity and expiry claims from OAuth tokens; refreshes expiring tokens; persists rotations atomically with owner-only permissions; and maps usage windows directly into the applet schema. Account identity is redacted before publication to QML.

The Claude adapter reads the selected Claude Code `.credentials.json`, including `CLAUDE_CONFIG_DIR` and `CLAUDE_SECURESTORAGE_CONFIG_DIR` profile boundaries. It refreshes expiring credentials through Anthropic's OAuth token endpoint, atomically updates the shared credential file, and maps session, weekly, model-scoped, routines, monthly-cap, and spend-limit data. No Claude executable or browser session is used.

The Gemini adapter reads Gemini CLI's `oauth_creds.json` and authentication selection. It resolves the CLI's public installed-app OAuth client from environment overrides or installed JavaScript, without starting Gemini or another process. The adapter renews and atomically persists access tokens, loads Code Assist tier and project metadata, optionally discovers a suitable Cloud project, and maps the most constrained Pro, Flash, and Flash Lite quota buckets. It identifies Google's June 2026 consumer-tier shutdown while leaving Workspace, education, and licensed Code Assist accounts enabled.

The xAI adapter uses a Management API key and explicit team ID from the process environment until KWallet-backed provider settings are available. It reads the team's posted prepaid balance and requests a best-effort 30-day daily spend series. Billing authentication failures invalidate the refresh, while analytics, parse, network, timeout, and size failures preserve a valid balance. The adapter does not treat prepaid balance as spend or share credentials with the separate Grok consumer service.

The Kimi adapter targets Kimi For Coding rather than the Moonshot/Kimi Open Platform. It prefers `KIMI_CODE_API_KEY`, then securely reads a fresh access token and stable device identity from the official Kimi Code CLI home. CLI credentials remain read-only because Kodometer neither invokes the CLI nor uses its refresh token. A rejected explicit key receives one retry with a fresh CLI credential. The adapter maps the Code API's 7-day request allowance and first short-window rate limit; it does not inspect browser cookies or accept `KIMI_AUTH_TOKEN`.

All network adapters must use fixed HTTPS endpoints in production, disable automatic redirects, impose request timeouts and response-size limits, avoid logging credentials, and test success, authentication renewal, malformed data, network failure, timeout, and oversized-response paths against local servers.

## Consequences

- Kodometer has no CodexBar runtime dependency.
- Provider behavior and release cadence are controlled by this project.
- Each provider requires dedicated authentication, parsing, security review, and maintenance.
- Credential-file compatibility can change when upstream tools change their formats.
- Native networking keeps refreshes asynchronous and works equally under Wayland and X11.
- KWallet becomes a required integration before the settings UI can accept manually entered secrets.
- New adapters can be introduced without changing QML as long as they produce the normalized provider map.

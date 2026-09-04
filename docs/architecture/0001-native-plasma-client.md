# ADR 0001: Native Plasma client over the CodexBar dashboard contract

- Status: Accepted
- Date: 2026-09-03

## Context

CodexBar already ships a Linux CLI with provider authentication, usage collection, cost scans, status data, and a versioned dashboard-v1 JSON contract. Reimplementing those provider clients would duplicate credential handling and would drift as provider APIs change.

A Plasma widget still needs a process boundary that does not rely on the deprecated executable data engine or shell command construction. It also needs testable state transitions and forward compatibility with additive dashboard fields.

## Decision

Build a compiled Plasma 6 applet with a QML interface and a small C++ backend.

The backend starts `codexbar dashboard` directly with `QProcess`. It requests redacted identity by default, limits stdout to 4 MiB, applies a hard timeout, validates schema version 1, and retains the last valid snapshot after a failed refresh. It preserves unrecognized JSON fields so new provider data remains available to QML before typed presentation support is added.

The applet targets Plasma 6 and Qt 6.4 or newer on Linux. It does not call Wayland or X11 APIs directly. Plasma owns panel placement, input, compositing, and session integration.

## Consequences

- CodexBar CLI is a runtime dependency and remains responsible for provider credentials.
- Provider additions in the CLI can appear in the generic UI without a C++ release.
- A stable dashboard schema change requires a parser update and tests.
- The compiled applet must be packaged for each CPU architecture and compatible Qt/KDE ABI.
- KDE Store distribution is less portable than a QML-only plasmoid because the package contains native code.

# Kodometer agent instructions

These are the repository owner's standing instructions for coding agents. Read this file at the start of every session. New explicit user instructions take precedence. Preserve these rules when conversation history is compacted; update this file when the owner changes the workflow.

## GitHub identities and authority

Use the following account split. Do not use the bot for routine GitHub reads, CI operations, comments, reviews, or repository administration.

| Operation | Command/account |
| --- | --- |
| Create any pull request, including release or back-merge PRs | `/home/kyau/bin/gh-bot` (`kyaulabs-bot`) |
| Merge an approved pull request | `/home/kyau/bin/gh-bot` (`kyaulabs-bot`) |
| Delete its eligible remote branch after confirming the merge | `/home/kyau/bin/gh-bot` (`kyaulabs-bot`) |
| Review/approve PRs, update Test Plan checkboxes, inspect CI, dispatch/watch workflows, read APIs, and all other GitHub operations | `gh` (`kyau`) |
| Ordinary local Git operations, signed commits, fetch/pull/push | `git` as `kyau` |

Before authenticated work, verify the identities with `gh api user --jq .login` and `/home/kyau/bin/gh-bot api user --jq .login`. Expect `kyau` and `kyaulabs-bot`, respectively. If an identity or permission is wrong, stop and report it. Do not switch accounts silently, expose credentials, or inspect secret values. Secret names can be checked without reading their contents.

The owner authorizes agents to perform the review and approval workflow below as `kyau`. Describe it honestly as an automated review under that authorized account; do not claim a separate human reviewed the change.

For every PR or issue you create or handle, automatically ensure **both `kyau` and `kyaulabs-bot` are assignees**, preserving any existing assignees. For every PR, also request **`kyau` as a reviewer** before review. Issues have no reviewer field. Perform these assignment/reviewer operations with `gh` as `kyau`, not `gh-bot`; PR creation itself still uses `gh-bot`. Check existing metadata to avoid duplicate review requests after `kyau` has already reviewed the current head. If GitHub disallows the requested reviewer (for example, a PR authored by `kyau`), report the restriction rather than inventing an approval or silently substituting an identity. Apply this default when triaging open work as well as creating new work; do not bulk-edit closed historical items without an explicit request.

## Required PR lifecycle

The current instruction is **complete, review, approve, merge, then clean up**. It replaces the earlier session instruction to open a completed PR and stop. Do not stop merely because the PR was created, unless the user explicitly asks for a review-only handoff or there is a blocker.

1. Inspect the working tree, base branch, existing PRs, and relevant documentation. Preserve unrelated user changes. Do not assume a previous PR was merged or a release published; check current state.
2. Work from current `develop` for features, fixes, and documentation. Use `<type>/kyau-$(openssl rand -hex 3)-description`, with lowercase kebab-case descriptions. Feature branches use `feat/`; use the appropriate Conventional Commit type for other work. Release branches are `release/X.Y.Z`.
3. Implement and validate the scoped change. Keep commits atomic, GPG-signed, and Conventional Commits compliant. Never use `--no-gpg-sign`, disable signing, or fall back to unsigned commits when pinentry fails. Ask for help unlocking the signing key if needed.
4. Create the PR with `gh-bot`. Features/fixes/docs target `develop`; release PRs target `main`; release back-merges target `develop`. Use `.github/PULL_REQUEST_TEMPLATE.md` and include an executable, concrete Test Plan. Distinguish performed checks from checks still pending. Then use `gh` to add both `kyau` and `kyaulabs-bot` as assignees and request `kyau` as reviewer. Assign both accounts to issues you create or handle too.
5. Using `gh` as `kyau`, inspect the final diff, commits, PR description, CI results, and Test Plan. Actually complete **every Test Plan task** before marking it done. Do not remove a failing task to manufacture completion. If a task cannot be performed, record the blocker and withhold approval/merge. Documentation-only work needs appropriate documentation checks, not invented runtime testing claims.
6. Wait for all applicable CI checks to finish successfully, including PR-only dependency review. Repository review/signature protections do not necessarily require status checks, so enforce this gate yourself. Recheck the head SHA; new commits require renewed validation and review.
7. Update completed Test Plan checkboxes with `gh`, then submit an approving review with `gh pr review --approve`. The review body must include the exact marker **`✔️ Approved by: @kyau`**, the reviewed head SHA, and concise evidence of completed checks. Never mark approval before the review and tests are complete.
8. Merge using `gh-bot`, with a merge commit to preserve signed history. Pin the validated SHA, for example `gh-bot pr merge <number> --merge --match-head-commit <sha>`. Do not use administrator bypasses, ignore failed checks, dismiss blocking reviews, or enable blind auto-merge of unreviewed future changes.
9. Confirm the PR's state is `MERGED` before branch cleanup. Cleanup is a **separate, subsequent operation**, not a combined merge-and-delete flag. Capture and validate the PR's head repository and branch before acting.
10. Delete the merged remote branch with `gh-bot` only if it belongs to this repository and is neither `main`, `develop`, nor a `release/X.Y.Z` branch. Conservatively preserve every `release/*` branch. Never delete a protected or unmerged branch. An explicit GitHub refs DELETE through `gh-bot api` is suitable; do not delete the remote branch through the `kyau` Git identity.
11. Fetch/prune and fast-forward the appropriate local integration branch with `git`. Never reset away user work. Local topic-branch removal, if useful, must use the safe merged-branch check (`git branch -d`), not forced deletion. Report the PR, merge, cleanup outcome, validation, and any remaining setup.

Apply this lifecycle to the task being performed. A request to document workflow does not itself request merging an unrelated pending release. Honor explicit holds on particular PRs. If permissions, reviews, CI, conflicts, or test failures prevent completion, explain the blocker instead of bypassing it.

## Release policy

- Use canonical SemVer `X.Y.Z` without leading zeroes. Base release branches on `develop`; keep `CMakeLists.txt`, `applet/metadata.json`, and `package.json` identical. Add `docs/releases/X.Y.Z.md` and update current documentation references.
- Starting with 0.3.0, every release overview must follow `docs/releases/TEMPLATE.md`: retain the same ordered emoji-headed sections, including sections with no changes. Keep the notes detailed enough to explain behavior, defaults, upgrade steps, compatibility limits, security, and validation. The publisher appends the final `📜 Changelog` section; generated version/commit groups are nested beneath it. Do not rewrite historical published notes to retrofit the template.
- Always include a `📦 Packages` table listing **every payload's exact versioned filename, Linux distribution/version or source-build purpose, and architecture**, plus the checksum inventory and verified AUR availability. Derive the table from `packaging/targets.json` and `scripts/package_metadata.py`, not remembered filenames. Run `tests/test_release_notes.py` and preview the actual combined overview/git-cliff output before approval.
- Choose the bump from actual Conventional Commits, not the branch name. `feat` normally means a minor release, `fix` a patch; assess breaking changes explicitly, including pre-1.0 policy. Do not recommend a patch release after adding `feat` commits without explaining a deliberate policy exception.
- Follow the PR lifecycle above for an authorized release task. **Merging a release PR into `main` triggers publication**; make this consequence clear. Keep release branches after merge. Never manually publish or create/move a real release tag during preparation.
- Read `docs/releasing.md`, `docs/packaging.md`, and the current release workflow before release changes. Validate the exact merge revision, canonical versions, annotated tag identity, bot permissions, and complete artifact inventory.
- Generate changelog notes with `--unreleased --tag` before the tag exists, or `--current` when retrying an already-tagged checkout. `--latest` can select the previous release. Preview real output, not just a mocked success.
- Draft assets can be retried only after validation. Download and verify every expected payload/checksum and compare draft bytes before publication. Published assets are immutable; never repair them by overwriting files or moving tags. Retry back-merges independently.
- Release workflow PR creation uses `KYAULABS_BOT_TOKEN`. This standing instruction does not authorize silently changing workflow credential routing or adding credentials to build jobs. Preserve least privilege and the checked-in release protocol.
- AUR publication is separately configured and opt-in through `AUR_PUBLISH_ENABLED`. It requires package ownership, dedicated SSH/GPG secrets, a verified host-key entry, and public signing identity variables. Never paste private keys into chat, issues, or PRs. Missing setup does not prevent generating recipes. Preserve signed AUR commits, strict host checking, no-force pushes, idempotency, and downgrade protection.
- Do not assume AUR credentials are configured from earlier conversation. Inspect variable/secret names and current state. Source recipe availability does not imply a live AUR entry.

## Product and security boundaries

Kodometer is a native Linux KDE Plasma widget for Wayland and X11. The repository is `kyaulabs/kodometer`; the checkout directory may still be named `codexbar-plasma`. The applet ID is `org.kyaulabs.kodometer`. Do not restore the old CodexBar namespace or CLI-backed architecture.

- Use Qt/QML presentation, C++20, and native asynchronous Qt Network. The compiled applet requires **Plasma/libplasma 6.4+**, not Plasma 6.0. Respect prefix-sensitive ECM installation paths and the explicit embedded-metadata build dependency.
- Never execute provider CLIs, inspect browser sessions/cookies, or add a runtime subprocess fallback. Maintainer/build automation is separate from the applet runtime.
- Securely reuse provider-owned credential files only after ownership, regular-file, symlink, size, and permissions checks. OAuth rotation must remain atomic and owner-only. Never copy credentials into another account's files.
- Manually entered credentials and private team/organization/project selectors belong in KWallet, not plaintext Plasma configuration, QML values, logs, notifications, or browser URLs. QML receives metadata, not saved secrets. Draft key fields are masked and cleared according to tested success/selection/lock behavior.
- Production endpoints are fixed official HTTPS destinations. Preserve bounded timeouts/responses, disabled automatic redirects, redacted errors/identity, and request context isolation. Browser opening is an explicit user action through `QDesktopServices` and an allowlisted URL catalog.
- Use only synthetic credentials and local fake servers in tests. Isolate D-Bus, HOME/config/runtime directories, and KWallet for smoke tests. Never install over a plugin loaded by the user's live desktop or send test notifications to that desktop.

## Account, polling, and presentation invariants

Read `docs/architecture/0002-native-provider-adapters.md`, README, and focused tests before touching these contracts. ADR 0001 is superseded.

- All eight providers are native: Codex, Claude, Gemini, DeepSeek, Kimi Code, OpenRouter, xAI, and z.ai/BigModel CN. Poll **only the selected account/profile per provider**, never every saved account concurrently.
- Codex/Claude/Gemini named profiles select existing credential folders. DeepSeek/Kimi/OpenRouter/xAI/z.ai named accounts select individual KWallet records, up to eight per provider (40 total). Do not introduce a shared wallet index or unnecessary migrations.
- Default retains its documented nonempty matching environment precedence and file/wallet discovery. Named selections are exclusive: missing keys, files, optional Management keys, or selectors must never inherit Default/environment/another account. Missing selections stay explicit; do not silently reset to Default.
- Wallet writes/removals are immediate and shared. Settings selections/preferences follow Apply/Cancel/Defaults. In-widget account changes save immediately for that widget via KConfig `writeConfig()`, preserve controller bindings, and survive Cancel in an already-open settings dialog. A later Apply can replace them with staged selections.
- Invalidate only the affected provider on selection, key, selector, or availability changes. Drop old snapshots/errors/history and reject stale/reentrant results, including A → B → A. Label-only and unselected-account edits do not refetch.
- Pin OAuth file paths and all request credentials/selectors before callbacks can change context. Renewal always updates the originating profile. Keep wallet read/session revisions and atomic state publication; late replies or close-during-mutation must not restore stale secrets.
- Manual Refresh can reopen/reload the wallet; automatic polling never reopens it. Failed Default reads clear Default state without disabling valid named accounts; retries/backend updates can recover it.
- Poll with a completion-based single-shot timer, no overlaps, a five-minute default, and bounds of 1–1440 minutes. No startup-setter networking. All-disabled means no polling. Context changes after the first cycle may queue a refresh even with automatic polling off.
- Account switching is metadata-only until an explicit selection. Pending navigation placeholders must contain no old usage and must never reach the compact quota meter or notifications.
- Notifications are off by default and use fresh successful results, never retained snapshots. One low-quota episode per provider; recovery requires threshold +5 percentage points. Invalid data cannot prove recovery. No private labels/selectors in notifications.
- Cost history is transient, reported-only UTC data. Unknown days are not zero. Do not add billing-history persistence, requests, synthetic provider totals, or a Qt Charts dependency.

### Provider-specific contracts

- Codex and Claude securely reuse and rotate their originating OAuth files. Codex also accepts its documented file API-key form.
- Gemini named profiles pin both `oauth_creds.json` and read-only `settings.json`. Shared public OAuth-client discovery may read installed JS but never execute it. API-key/Vertex authentication and retired consumer tiers are not Code Assist OAuth quota support.
- Kimi is For Coding, not Moonshot/Open Platform. CLI fallback is Default-only and read-only; never use its refresh token or rewrite `kimi-code.json`. Creating a missing owner-only device ID is allowed.
- DeepSeek uses the public balance endpoint and supports funded non-USD currencies. Do not replace this with browser/private billing endpoints.
- xAI is developer Management API billing, not Grok/SuperGrok. Pin the key/team pair. History authentication failures are fatal; other optional history failures retain valid balance.
- OpenRouter credits are authoritative; a key cap is not the balance. Management Activity is optional, separate, bounded, and never inherited by named accounts. Do not publish partial or conflicting Activity totals.
- z.ai regions and personal/team scope are explicit. Never send CN credentials to global endpoints or team headers to balance. Quota is authoritative; analytics/CN balance are optional. Private selectors remain in C++/KWallet; normalized public region labels can enter presentation/actions.

## Validation and implementation discipline

- Work test-first for behavioral changes: reproduce red, implement green, then refactor. Prefer focused tests plus full validation. Documentation-only edits do not require manufacturing a failing code test.
- Keep **96% minimum line, function, and branch coverage**. Never lower a gate, add exclusions, or filter failures to conceal missing tests. Scan only the instrumented build directory; retain `merge-mode-functions = merge-use-line-min`.
- Test security boundaries, malformed data, ownership/permissions, failure/timeout/oversize paths, staging, stale results, and reentrancy. Simplify nested Qt temporary expressions when appropriate rather than concealing branches.
- Test optimized builds too. Do not place side-effecting operations such as `server.listen()` inside `Q_ASSERT`; release builds remove those expressions. Assert version-bearing protocol fields against `KODOMETER_VERSION`, not a stale literal.
- QML-exposed C++ classes cannot be `final`; generated QML types subclass them. Update both core and applet explicit source lists when adding classes.
- `CredentialStore` owns its backend: use a heap-allocated fake, not a stack object. Construct a one-adapter controller explicitly as `UsageController(QList<ProviderAdapter *>{adapter})` to avoid selecting the QObject-parent overload.
- Compiled QML resources use URI `plasma.applet.org.kyaulabs.kodometer` and prefix `qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/`. ECM flattens configuration QML resources. Repeater-generated visuals should be inspected through `QQuickItem::childItems()`. Keep plain-text handling for provider/account labels.
- The config schema, API limits, exact diagnostics, and provider fixtures already live in source/tests. Read them rather than duplicating their contents in session notes or changing their meaning accidentally.

### Useful commands

Use current checked-in scripts and workflow dependencies as the source of truth. `npm test` is not the native test entry point.

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
cmake --build build --target org.kyaulabs.kodometer_qmllint
python3 -m unittest discover -s tests -p 'test_*.py'
scripts/check-format.sh
shellcheck scripts/*.sh
scripts/coverage.sh
pnpm exec commitlint --from origin/develop --to HEAD
gitleaks git --no-banner --redact
```

Also run Actionlint and the appropriate PR CI jobs. For release PRs, lint from `origin/main` and run `scripts/validate-release.sh release/X.Y.Z`. Use fresh Release-mode builds and the complete distro matrix for packaging/release changes. Record actual results; test totals and coverage percentages are snapshots, not substitutes for rerunning checks on the reviewed SHA.

## Packaging and local environment

- Release packaging is **x64 only**: AUR/RPM `x86_64`, DEB `amd64`. No ARM matrix or cross-distro repackaging of Arch binaries.
- `packaging/targets.json` defines supported images. The policy is compatible Ubuntu LTS and supported newer Ubuntu releases, plus supported stable Fedora releases, all with Plasma 6.4+. Verify current distro support/development packages before editing the matrix; do not infer generic Debian/RHEL compatibility.
- `.github/workflows/packages.yml` builds and tests native binaries, then installs/reinstalls them in separate runtime containers without SDK dependencies. Preserve native dependency scanning and explicit dynamic QML/wallet requirements.
- `scripts/package_metadata.py` defines the complete release inventory and checksums. Keep source/AUR provenance and exact file allowlists. A package manager install must not delete credentials or preferences. APT/DNF repository hosting/signing is separate work.
- Provision/build/install helpers are for disposable containers, not the owner's desktop. `scripts/package.sh` produces only the legacy archive, not a complete multi-format release bundle.
- The owner's checkout has historically been `/home/kyau/projects/kyaulabs/codexbar-plasma` on Arch x86_64. Local builds have required `CMAKE_PREFIX_PATH=/tmp/ecm/usr`. Check that this extracted ECM prefix still exists before using it; do not put temporary paths into CI or committed package metadata.
- Optional local tools have lived at `/tmp/kodometer-gcovr/bin` and `/tmp/actionlint-pkg/usr/bin/actionlint`. Treat `/tmp` helpers, builds, screenshots, and logs as disposable, not durable dependencies. Recreate missing tools safely or use the checked-in CI environments; do not claim tests passed because an old log exists.
- For unreliable multiplexed SSH connections, use `GIT_SSH_COMMAND='ssh -S none -o ControlMaster=no -o BatchMode=yes -o ConnectTimeout=15'` with ordinary Git operations. Remote branch cleanup still belongs to `gh-bot`.
- Compiled applets are not `kpackagetool6` ZIPs. Respect system versus user-local prefixes, avoid duplicate plugins, and stop Plasma before replacing a loaded binary. See README for safe installation/uninstallation.

## Writing and durable handoffs

Be concise and specific. State blockers and unverified behavior without pretending success. Use clear file paths and measured validation results.

Before substantial prose, release notes, runbooks, or PR descriptions, load `/home/kyau/.pi/agent/npm/node_modules/@kyaulabs/prism-core/skills/distill/SKILL.md`; for substantial rewriting, also read its `references/patterns.md`. If unavailable, report that and retain direct, precise prose without inventing a replacement skill.

Keep durable rules here and domain details in README, `CONTRIBUTING.md`, the accepted ADR, `docs/packaging.md`, and `docs/releasing.md`. Do not preserve secrets, personal credential contents, huge tool transcripts, obsolete PR states, or temporary build artifacts as instructions. At session resumption, inspect Git/GitHub state instead of trusting remembered branch, tag, release, or credential status.

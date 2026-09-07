# Preparing and recovering a release

Releases use `release/X.Y.Z` branches from `develop`, merged into `main`. Merge with a merge commit to retain the reviewed, signed history. Do not tag or publish from the unmerged release branch.

## Before merge

1. Match the canonical `X.Y.Z` version in `CMakeLists.txt`, `applet/metadata.json`, and `package.json`. Leading-zero versions and prerelease branch suffixes are rejected.
2. Add a reader-facing overview in `docs/releases/X.Y.Z.md`. The workflow prepends it to the generated git-cliff changelog when creating the release draft.
3. Run `scripts/validate-release.sh release/X.Y.Z`, native tests, coverage, QML/format checks, and the offline automation tests:
   ```bash
   python3 -m unittest discover -s tests -p 'test_*.py'
   ```
   These tests need Python 3.11 or newer, Bash, jq, CMake, Git, and GnuPG. GitHub and release-tag mutations are mocked. AUR tests use local bare repositories and disposable GPG keys, never the real AUR or a maintainer's signing key.
4. Check the [distribution matrix](packaging.md). CI must build every target, run its Release-mode native tests, install and reinstall the package in a separate runtime container, and pass the isolated Plasma startup check. Download the `kodometer-release-bundle` CI artifact to inspect the complete payload and checksums. Keep the 96% coverage gates unchanged.
5. Open the release PR against `main` and wait for every CI job to pass. Obtain review before merging. A signed-commit/review rule is not a required-status-check rule; do not assume GitHub will prevent a merge while CI is failing or still running.
6. Confirm that the publication job can write repository contents and that `KYAULABS_BOT_TOKEN` can read and create repository pull requests. Secret-name visibility alone does not prove token validity or write permission. Do not put token values in issues, PRs, or command output.
7. If AUR publication is enabled, check its dedicated credentials and package ownership before merging. AUR setup is independent of GitHub package generation.

The CI conventions job runs the offline tests and validates the release version before a PR into `main` can pass that job. Native package generation waits for conventions, native tests, coverage, and security checks. Repository rules should require the CI checks if merge-time enforcement is desired.

## After merge

The Release workflow accepts only a merged `release/` PR from this repository into `main`. It checks out the exact merge commit and uses Bash error propagation for version validation.

1. Check the version and back-merge credential, then invoke the shared native-package workflow. Builds use the target distribution's libraries; DEB/RPM files never reuse Arch binaries.
2. Collect the source archive, AUR recipe bundle, Arch package, legacy Arch tar archive, Ubuntu DEB, and Fedora RPMs. Generate an individual `.sha256` for each payload and a complete `kodometer-X.Y.Z-SHA256SUMS` manifest.
3. `scripts/publish-release.sh` validates version, repository, merge SHA, clean tracked files, API access, and the complete local inventory before creating a tag. Existing tags must be annotated and point to that same commit. Tags are never forced or moved.
4. Generate notes, push the annotated tag, and create or resume a GitHub draft release. Upload only the explicit artifact inventory. Missing, unexpected, or malformed local assets fail validation rather than being silently omitted.
5. Download every expected draft asset, verify the complete manifest and individual checksums, and compare every downloaded file with its local input. Publish only after all checks succeed.
6. Open a `main` → `develop` back-merge PR using the bot token, or reuse the existing open PR. Review and merge it so the branches share release history.
7. If `AUR_PUBLISH_ENABLED` is `true`, run the separate AUR job after GitHub publication and back-merge creation succeed. That job downloads the published source and recipes, verifies their hashes against the manifest, compares `PKGBUILD` with the release's template, and regenerates `.SRCINFO` before exposing publication credentials to the update step.

The source tag can be visible before the draft assets are published. Tags are annotated, not independently GPG-signed by this workflow. DEB/RPM packages are not independently signed either. Checksums verify integrity, not publisher identity. AUR metadata commits use the separately configured GPG signing key.

## AUR publication setup

Recipe generation and Arch package builds work without an AUR account. Automatic AUR pushes are disabled unless the repository variable `AUR_PUBLISH_ENABLED` is exactly `true`.

1. Create or select the AUR account that will own or co-maintain `kodometer`. Confirm the package name is available or that the account can update the existing package. The first successful push can create the AUR entry; do not enable automation before agreeing on ownership.
2. Add a dedicated automation SSH public key to that AUR account. Store its private key in the GitHub Actions secret `AUR_SSH_PRIVATE_KEY`. The job uses noninteractive SSH; an SSH key that requires an unavailable agent or password will fail.
3. Set `AUR_SSH_KNOWN_HOSTS` to a verified OpenSSH known-hosts entry for `aur.archlinux.org`. Compare its fingerprint with a trusted Arch source. Do not trust an unverified `ssh-keyscan` result. The script requires strict host-key checking and does not fall back to password authentication.
4. Create a dedicated GPG signing key for this automation, not a personal primary key. Store its ASCII-armored secret-key export in `AUR_GPG_PRIVATE_KEY` and its passphrase, if any, in `AUR_GPG_PASSPHRASE`. Publish the public key where maintainers can verify it.
5. Set repository variables `AUR_SIGNING_KEY` to the full 40-hex signing-key fingerprint, and `AUR_COMMIT_NAME` and `AUR_COMMIT_EMAIL` to the bot's public commit identity.
6. Enable `AUR_PUBLISH_ENABLED` only after the secrets, variables, ownership, and permissions are ready. Use GitHub's secret settings, not issues, PR bodies, shell tracing, or chat, to provide private values.

`scripts/update-aur.sh` changes only `PKGBUILD` and `.SRCINFO` in the fixed `kodometer` AUR repository. It imports the signing key into a temporary private GnuPG home, creates a GPG-signed Conventional Commit, verifies the signature, and pushes without force. Signing failures never produce an unsigned fallback. Temporary key files and the signing agent are removed on exit.

## Retry behavior

- Validation, token/API access, wrong-tag, and incomplete/corrupt local-inventory failures stop before remote release changes. Inspect the failure; do not move a tag to make a check pass.
- An interrupted tag push, draft creation, asset upload, verification, or publication can be retried from the same merged PR's workflow run. A draft's expected assets may be replaced on retry. Do not manually publish a draft while automation is uploading it.
- Once published, the script never replaces assets or edits the release. It downloads and verifies the original complete inventory and resumes only the back-merge operation. A later rolling-toolchain rebuild is not used to overwrite original binaries.
- Missing or corrupt published assets require maintainer investigation. The script refuses automatic repair of public assets.
- If back-merge creation failed, repair token permissions or repository policy and rerun. An existing back-merge PR is reused rather than duplicated.
- If only AUR publication failed, repair its credentials or repository state and rerun the failed AUR job. GitHub assets stay published and unchanged; the AUR job reads those public assets rather than rebuilding them.
- Identical AUR recipes are a no-op. Rerunning an older release never downgrades a newer AUR version. Different recipes at the same version, unexpected repository files, and concurrent non-fast-forward pushes stop for review instead of overwriting changes.

The `v0.1.0` workflow published only the legacy archive/checksum pair. Recover that release through its original workflow run and checked-out scripts, not by asking the new publisher to add native assets to it.

Build jobs have a 45-minute timeout, clean runtime jobs 20 minutes, publication 30 minutes, and AUR updates 15 minutes. Runs serialize by release branch, and AUR pushes also serialize across releases. CI artifacts expire after seven days; rebuild expired draft inputs through the original workflow rather than guessing checksums. No provider credentials or live provider requests are needed.

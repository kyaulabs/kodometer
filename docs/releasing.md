# Preparing and recovering a release

Releases use `release/X.Y.Z` branches from `develop`, merged into `main`. Merge with a merge commit to retain the reviewed, signed history. Do not tag or publish from the unmerged release branch.

## Before merge

1. Match the canonical `X.Y.Z` version in `CMakeLists.txt`, `applet/metadata.json`, and `package.json`. Leading-zero versions and prerelease branch suffixes are rejected. The first release already uses `0.1.0` in all three files.
2. Add a reader-facing overview in `docs/releases/X.Y.Z.md`. The workflow prepends it to the generated git-cliff changelog when creating the release draft.
3. Run `scripts/validate-release.sh release/X.Y.Z`, native tests, coverage, QML/format checks, and the offline publication tests:
   ```bash
   python3 -m unittest discover -s tests -p test_release.py
   ```
   These tests need Python 3, Bash, and jq. All git, GitHub, and changelog mutations are mocked in temporary directories.
4. Build the archive and verify its checksum, embedded metadata, linkage, and isolated Plasma startup. Check system and user-local installation behavior without overwriting a plugin loaded by the current desktop.
5. Open the release PR against `main` and wait for every CI job to pass. Obtain review before merging. A signed-commit/review rule is not a required-status-check rule; do not assume GitHub will prevent a merge while CI is failing or still running.
6. Confirm that the workflow can write repository contents and that `KYAULABS_BOT_TOKEN` can read and create repository pull requests. Secret-name visibility alone does not prove token validity or write permission. Do not put token values in issues, PRs, or command output.

The CI conventions job runs the offline publication tests and validates the release version before a PR into `main` can pass that job. Package generation also waits for conventions. Repository rules should require the CI checks if merge-time enforcement is desired.

## After merge

The Release workflow accepts only a merged `release/` PR from this repository into `main`. It checks out the exact merge commit and uses Bash error propagation for version validation.

1. Check for the back-merge credential, then build the release archive.
2. `scripts/publish-release.sh` validates version, repository, merge SHA, clean tracked files, API access, and the local checksum before creating a tag. Existing tags must be annotated and point to that same commit. Tags are never forced or moved.
3. Generate notes, push the annotated tag, and create or resume a GitHub draft release. Upload only the expected architecture-specific archive and its checksum; unrelated `dist/` files are not uploaded.
4. Download both draft assets, verify the checksum's exact filename and digest, and compare the downloaded bytes with the local files. Publish only after verification succeeds.
5. Open a `main` → `develop` back-merge PR using the bot token, or reuse the existing open PR. Review and merge it so the branches share release history.

The source tag can be visible before the draft assets are published. Tags are annotated, not independently GPG-signed by this workflow. Archive checksums verify integrity, not publisher identity.

## Retry behavior

- Validation, token/API access, wrong-tag, and local-checksum failures stop before remote release changes. Inspect the failure; do not move a tag to make a check pass.
- An interrupted tag push, draft creation, asset upload, verification, or publication can be retried from the same merged PR's workflow run. A draft's expected assets may be replaced on retry. Do not manually publish a draft while automation is uploading it.
- Once published, the script never replaces assets or edits the release. It downloads and verifies the published archive/checksum pair and resumes only the back-merge operation. A later rolling-toolchain rebuild is not used to overwrite the original binaries.
- A missing or corrupt published pair requires maintainer investigation. The script refuses automatic repair of public assets.
- If publication succeeded but back-merge creation failed, repair token permissions or repository policy, then rerun. An existing back-merge PR is reused rather than duplicated.

The workflow has a 30-minute timeout and serializes runs for the same release branch. No provider credentials or live provider requests are needed for publication.

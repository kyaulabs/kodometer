# Contributing

Thanks for taking an interest in this project. We want to make contributing to this project as easy and transparent as possible, whether it is:

* Reporting a bug
* Discussing the current state of the code
* Submitting a fix
* Proposing new features
* Becoming a maintainer

## We develop with GitHub

We use GitHub for source control, issues, feature requests, and pull requests. Discussion and general support take place on Discord.

## We Use [Git Flow](https://www.gitkraken.com/learn/git/git-flow)

<div align="center" style="background:#0d1117"><img src=".github/media/git-flow.svg" width="240" height="365" style="margin-bottom:2ch" /></div>

All code changes happen through pull requests and are the best way to propose changes to the codebase. We actively welcome your pull requests:

1. Fork the repository and branch from `develop`.
2. Name the branch `<type>/<name>-<hash>-<description>`, for example `feat/kyau-a1b2c3-provider-tabs`.
   * `<type>` matches the intended Conventional Commit type.
   * `<name>` is your GitHub username.
   * Generate `<hash>` with `openssl rand -hex 3`.
   * Write `<description>` in lowercase kebab case.
3. Work test-first: add a failing test, make it pass, then refactor while the suite remains green.
4. Keep commits atomic, signed, and compliant with Conventional Commits.
5. Update documentation when behavior or public interfaces change.
6. Run formatting, lint, tests, and coverage before pushing.
7. Open the pull request against `develop` and complete the repository template.

## Reporting bugs and feature requests

Use [GitHub Issues](/../../issues) to report a bug or request a feature. Include reproducible steps, the Plasma and Qt versions, the CodexBar CLI version, and relevant logs with credentials removed.

## Contributions & Software Licensing

In short, when you submit code changes, your submissions are understood to be under the same [license](LICENSE) that covers the project itself. If you have a concern about this, please refrain from submitting a PR and contact a maintainer directly.

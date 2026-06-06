---
title: "GitHub Actions: Launchpad PPA Build + Upload"
type: concept
sources:
  - raw/sessions/ppa-upload-session-2026-06-06.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/launchpad-ppa-workflow.md
confidence: medium
updated: 2026-06-06
---

# GitHub Actions: Launchpad PPA Build + Upload

Workflows for packaging `pinba_engine` into Debian source packages for multiple Ubuntu
releases and publishing them to `ppa:xolegator/packages` from GitHub Actions.

## What the workflow does

1. Checks out the repository.
2. Generates distro-specific `debian/pinba-ppa-build.mk` so the source package knows
   which MySQL series to build on Launchpad.
3. Rewrites `debian/changelog` to a distro-specific version such as `2.1.2-1~noble1`
   or `2.1.2-1~resolute1`.
4. Creates `pinba-engine_<upstream>.orig.tar.gz` with `git archive`.
5. Builds the source package with `dpkg-buildpackage -S -sa -us -uc`.
6. Runs `lintian` on the generated `.changes` file.
7. Signs the `.dsc` and `.changes` files with the Launchpad GPG key.
8. Uploads the signed source package to Launchpad with `dput` over SFTP.

## Required secrets

| Secret | Purpose |
|--------|---------|
| `LAUNCHPAD_GPG_PRIVATE_KEY` | Base64-encoded armored private key used for package signing |
| `LAUNCHPAD_GPG_PASSPHRASE` | Passphrase for the signing key |
| `LAUNCHPAD_GPG_FINGERPRINT` | Exact key fingerprint passed to `gpg` |

## Design notes

- The workflow uses a distro matrix (`noble`, `resolute`), but not a separate GitHub
  matrix over MySQL series. Instead, each source package carries a generated
  `debian/pinba-ppa-build.mk` file, and `debian/rules` uses that to decide whether
  to build `pinba-engine-mysql-8.0`, `pinba-engine-mysql-8.4`, or both.
- GitHub Actions cannot safely filter a matrix job with `job.if` conditions that
  reference `matrix.*`. That condition is validated before matrix expansion, which
  causes workflow-file errors on branch push. Dynamic distro selection must be done
  by generating the matrix in a preceding job and passing it through job outputs.
- `git archive` is required because `dpkg-buildpackage -S -sa` expects the matching
  `orig.tar.gz` one directory above the package tree.
- When source-package artifacts move between jobs, the workflow must preserve
  `*.buildinfo` alongside `.dsc`, `.changes`, `.orig.tar.gz`, and `.debian.tar.*`.
  `debsign` expects the matching `*_source.buildinfo` next to the `.changes` file and
  fails before SSH upload if that file is missing.
- Launchpad's official docs support both FTP and SFTP uploads. For GitHub-hosted
  automation, plain FTP with `login = anonymous` is the simpler and more robust path
  because it avoids separate SSH-key provisioning for the runner while preserving GPG
  signature verification on the uploaded source package.
- SFTP remains possible, but it adds two extra moving parts on GitHub Actions:
  `python3-paramiko` on the runner and a Launchpad-registered SSH key with matching
  secret material in GitHub.

## MySQL version monitoring

Separate workflow: `.github/workflows/mysql-version-monitor.yml`

What it does:
- runs weekly and on manual dispatch
- checks `libmysqlclient-dev` versions inside `ubuntu:24.04` and `ubuntu:26.04`
- compares them with `.github/mysql-versions.json`
- opens a GitHub issue if the tracked MySQL availability changed

This workflow does not auto-upload to Launchpad because a new PPA upload still requires
an explicit Debian revision bump.

## Triggering

- `release.published` for automatic PPA publication from a GitHub Release.
- `workflow_dispatch` for manual rebuilds and version overrides.

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

Workflow for packaging `pinba_engine` into a Debian source package and publishing it
to `ppa:xolegator/packages` from GitHub Actions.

## What the workflow does

1. Checks out the repository.
2. Optionally rewrites `debian/changelog` when a manual `debian_version` override is provided.
3. Creates `pinba-engine_<upstream>.orig.tar.gz` with `git archive`.
4. Builds the source package with `dpkg-buildpackage -S -sa -us -uc`.
5. Runs `lintian` on the generated `.changes` file.
6. Signs the `.dsc` and `.changes` files with the Launchpad GPG key.
7. Uploads the signed source package to Launchpad with `dput` over SFTP.

## Required secrets

| Secret | Purpose |
|--------|---------|
| `LAUNCHPAD_GPG_PRIVATE_KEY` | Base64-encoded armored private key used for package signing |
| `LAUNCHPAD_GPG_PASSPHRASE` | Passphrase for the signing key |
| `LAUNCHPAD_GPG_FINGERPRINT` | Exact key fingerprint passed to `gpg` |
| `LAUNCHPAD_SSH_PRIVATE_KEY` | SSH key used by `dput` for Launchpad SFTP upload |

## Design notes

- The repo’s `debian/rules` builds both `pinba-engine-mysql-8.0` and
  `pinba-engine-mysql-8.4` from one source package, so the workflow does not need
  a MySQL-series matrix.
- `git archive` is required because `dpkg-buildpackage -S -sa` expects the matching
  `orig.tar.gz` one directory above the package tree.
- `dput` should be configured for `method = sftp`; Launchpad upload docs and local
  testing both pointed to SSH-based upload as the reliable path.
- Uploading with SFTP still needs Launchpad SSH key registration, even though the
  package signature itself is GPG-based.

## Triggering

- `release.published` for automatic PPA publication from a GitHub Release.
- `workflow_dispatch` for manual rebuilds and version overrides.

---
title: "GitHub Actions: Launchpad PPA Build + Upload"
type: concept
sources:
  - raw/sessions/ppa-upload-session-2026-06-06.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/launchpad-ppa-workflow.md
confidence: high
updated: 2026-07-15
---

# GitHub Actions: Launchpad PPA Build + Upload

Workflows for packaging `pinba_engine` into Debian source packages for multiple Ubuntu
releases and publishing them to `ppa:xolegator/packages` from GitHub Actions.

## What the workflow does

1. Checks out the repository at the appropriate ref (tag for releases, explicit `source_ref`
   for manual runs, or `github.sha` for push-triggered runs).
2. Generates distro-specific `debian/pinba-ppa-build.mk` so the source package knows
   which MySQL series to build on Launchpad.
3. Rewrites `debian/changelog` to a distro-specific version such as `2.3.0-1~noble1`
   or `2.3.0-1~resolute1`. The debian revision comes from `.github/mysql-versions.json`
   (`debian_revision` field) unless overridden by the manual `debian_revision` input.
4. Obtains `pinba-engine_<upstream>.orig.tar.gz`: first tries to download the already
   published orig for this upstream version from the PPA pool
   (`https://ppa.launchpadcontent.net/<owner>/<ppa>/ubuntu/pool/main/p/pinba-engine/`);
   only if none is published yet (a new release) does it generate one with
   `git archive` (preferring the `v<upstream>` tag) piped through `gzip -9n`.
5. Assembles the source tree by unpacking the orig and replacing its `debian/` with the
   checkout's `debian/`, then builds the source package with
   `dpkg-buildpackage -S -sa -us -uc` (`-sd` for series that must not re-carry the orig).
6. Runs `lintian` on the generated `.changes` file.
7. Signs the `.dsc` and `.changes` files with the Launchpad GPG key.
8. Uploads the signed source package to Launchpad with `dput` over plain FTP
   with `passive_ftp = 1`, using a retry loop (3 attempts, 30 s gap) for resilience.

## Triggering

- `release.published` — automatic PPA publication from a GitHub Release.
- `push` to `master` where `.github/mysql-versions.json` changed — triggered by the
  MySQL version monitor auto-merge PR; `debian_revision` is read from that file.
- `workflow_dispatch` — manual rebuilds and version overrides.

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
- `dpkg-buildpackage -S -sa` expects the matching `orig.tar.gz` one directory above the
  package tree; the workflow places the downloaded (or generated) orig there itself.
- **Rebuilds of an already-released upstream version must reuse the published orig
  byte-for-byte.** Regenerating it is not byte-stable: for the same tag the `git archive`
  tar layer is deterministic, but the gzip layer differs across environments/gzip builds
  (verified empirically on 2.11.3 — identical tar, different `.gz`). This is why the
  workflow downloads the published orig from the PPA pool instead of re-archiving.
  This bit both `2.2.0` (manual dispatch, 2026-06-06) and `2.11.3-2/-3`
  (push-triggered rebuild from a later `master` commit, 2026-07-14).
- Building straight from the master checkout with an older orig fails in
  `dpkg-source` with "unexpected upstream changes" for any post-release drift outside
  `debian/`. The workflow therefore unpacks the orig and overlays only `debian/` from
  the checkout, so the source tree is pristine-upstream + current packaging by
  construction (no auto-generated quilt patches either).
- When source-package artifacts move between jobs, the workflow must preserve
  `*.buildinfo` alongside `.dsc`, `.changes`, `.orig.tar.gz`, and `.debian.tar.*`.
  `debsign` expects the matching `*_source.buildinfo` next to the `.changes` file and
  fails before upload if that file is missing.
- For multi-series uploads of the same upstream version, only one source package should
  carry the full `.orig.tar.gz` (`-sa`). Additional series uploads should use `-sd`,
  otherwise Launchpad may reject repeated orig uploads or behave inconsistently on the
  second transfer.
- Each `*_source.changes` is uploaded with a separate `dput` invocation. Sending
  multiple series through one shell-expanded command makes failure attribution harder
  and can interact badly with per-upload connection state on the server side.
- The declared `upstream_version` must match the checked-out source tree used for
  `git archive`. Rebuilding `2.2.0` from a later `master` commit produces a different
  `orig.tar.gz` and potentially a different `debian.tar.*`, which Launchpad rejects for
  the same version. Manual upload flows therefore package an explicit tag or SHA,
  not the moving branch tip.
- If a wrong `orig.tar.gz` was already uploaded to the same PPA under a given upstream
  version, that upstream version is effectively burned for future corrected uploads in
  that archive. The correct recovery is to cut a new upstream release so the orig
  filename changes naturally and the archive sees a clean version namespace.
  In practice, `v2.2.1` fixed the `2.2.0` conflict.

## Launchpad upload transport

Launchpad's official documentation supports both FTP and SFTP for `dput` uploads.
For GitHub-hosted automation, plain anonymous FTP with `passive_ftp = 1` is used:

```ini
[xolegator-packages]
fqdn            = ppa.launchpad.net
method          = ftp
incoming        = ~xolegator/packages/ubuntu/
login           = anonymous
allow_unsigned_uploads = 0
passive_ftp     = 1
```

`passive_ftp = 1` is essential: GitHub Actions runners are behind NAT, so the
server cannot open a data connection back to the runner (active FTP). Passive mode
reverses that: the client initiates the data connection, which NAT allows.

SFTP is possible but adds two extra moving parts: `python3-paramiko` on the runner
and a Launchpad-registered SSH key with matching secret material in GitHub.
FTP with `passive_ftp = 1` avoids all of that while preserving GPG signature
verification on the uploaded source package.

A retry loop (3 attempts, 30 s gap) is wrapped around each `dput` call for
resilience against transient Launchpad FTP errors.

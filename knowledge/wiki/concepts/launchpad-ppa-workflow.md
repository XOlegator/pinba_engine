---
title: "Launchpad PPA Upload Workflow"
type: concept
sources:
  - raw/sessions/ppa-upload-session-2026-06-06.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/github-actions-ppa.md
confidence: high
updated: 2026-06-07
---

# Launchpad PPA Upload Workflow

Practical guide to signing and uploading a source package to the Launchpad PPA,
including known pitfalls. This covers both the manual (local) flow and the
GitHub Actions automated flow.

## Prerequisites

| Requirement | Where |
|-------------|-------|
| GPG key registered on Launchpad | https://launchpad.net/~{user}/+editpgpkeys |
| `devscripts`, `dput` installed | `sudo apt install devscripts dput` |

## dput configuration (manual/local uploads)

For local uploads from a developer workstation, SFTP is the recommended transport
because plain FTP port 21 is commonly blocked by ISPs and home routers.

```ini
# ~/.dput.cf
[xolegator-packages]
fqdn            = ppa.launchpad.net
method          = sftp
incoming        = ~xolegator/packages/ubuntu/
login           = xolegator
allow_unsigned_uploads = 0
```

```
# ~/.ssh/config
Host ppa.launchpad.net
    User xolegator
    IdentityFile ~/.ssh/id_ed25519_launchpad
    IdentitiesOnly yes
```

Connectivity check: `ssh -T xolegator@ppa.launchpad.net` → `No shells on this server.` ✓

For GitHub Actions automation, plain FTP with `passive_ftp = 1` is used instead.
See [[github-actions-ppa]] for the automated transport configuration.

## Full upload cycle

```bash
# 1. Create orig.tar.gz from git (only when upstream changed)
git archive --prefix=pinba-engine-{VERSION}/ HEAD | gzip -9 > ../pinba-engine_{VERSION}.orig.tar.gz

# 2. Build source package (no compilation, no signing)
dpkg-buildpackage -S -sa -us -uc [-d]
#   -S   = source only
#   -sa  = include orig.tar.gz
#   -us  = unsigned .changes
#   -uc  = unsigned .dsc
#   -d   = skip dependency check (if Build-Depends are not installed locally)

# 3. Sign (requires interactive terminal for GPG pinentry)
debsign -k{FINGERPRINT} ../pinba-engine_{VERSION}_source.changes

# 4. Upload
dput xolegator-packages ../pinba-engine_{VERSION}_source.changes
```

After upload, wait for an email to the maintainer address: Launchpad confirms acceptance
or rejection.

## Version format

Format: `{upstream}-{deb_rev}~{ubuntu_codename}{n}`

| Case | Version |
|------|---------|
| First upload for noble | `2.1.2-1~noble1` |
| Packaging fix (same upstream) | `2.1.2-2~noble1` |
| New Ubuntu release | `2.1.2-1~resolute1` |

Rule: Launchpad **does not accept** re-uploading the same version. Any change requires
a revision bump (`-1` → `-2`).

In the automated flow, `debian_revision` is stored per-suite in `.github/mysql-versions.json`
and is incremented automatically by the MySQL version monitor workflow.

## Build monitoring

Build list: `https://launchpad.net/~xolegator/+archive/ubuntu/packages/+builds`

States: Needs building → Building → Successfully built / Failed to build

On failure: Launchpad sends a build log link in an email to the maintainer.

## PPA signing key propagation on first publish

After the first upload, Launchpad generates a PPA signing key. The key appears on
`keyserver.ubuntu.com` with a delay of 5–30 minutes.

Until the key propagates, `add-apt-repository` returns:
```
ServerError: HTTP Error 500
GPGKeyTemporarilyNotFoundError
```

This is a temporary error — wait and retry.

## Installing from PPA on client machines

```bash
# Standard method (wait for key propagation):
sudo add-apt-repository ppa:xolegator/packages
sudo apt update
sudo apt install pinba-engine-mysql-8.0

# If add-apt-repository created a duplicate source after manual addition:
sudo rm /etc/apt/sources.list.d/xolegator-packages.list
```

`add-apt-repository` creates a file in deb822 format (`.sources`); manual addition
creates the old-style format (`.list`). They duplicate each other — keep only one.

## Known Launchpad build failures

### rapidjson/fwd.h not found

Symptom:
```
fatal error: rapidjson/fwd.h: No such file or directory
In file included from sql/dd/sdi_fwd.h:27
```

Cause: MySQL 8.0/8.4 plugin headers pull in `rapidjson/fwd.h` via the chain
`ha_pinba.cc → sql/table.h → sql/dd/types/foreign_key.h → sql/dd/sdi_fwd.h → rapidjson/fwd.h`.
This file is not in `vendor/mysql-headers-*` and not in Build-Depends.
Local builds may succeed due to a stale cmake cache with full MySQL source;
Launchpad builds in a clean chroot and fail immediately.

Fix: add `rapidjson-dev` to Build-Depends in `debian/control`.

### UNRELEASED in changelog

Symptom: Launchpad rejects the upload (rejection letter to maintainer email).

Fix: the changelog distribution field must be a real Ubuntu codename (`noble`, `resolute`),
not `UNRELEASED`.

---
title: "PPA Upload Session — 2026-06-06"
type: source
sources:
  - raw/sessions/ppa-upload-session-2026-06-06.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/launchpad-ppa-workflow.md
confidence: high
updated: 2026-06-07
---

# PPA Upload Session — 2026-06-06

This session closed Stage 3 (orig tarball) and Stage 4 (upload to PPA) of the internal
PPA automation plan.

## Key facts

- **orig.tar.gz** is created manually via `git archive`, not by `dpkg-buildpackage`
- **UNRELEASED** in changelog is rejected by Launchpad — must use a real Ubuntu codename (`noble`)
- **Version format:** `2.1.2-1~noble1` (both MySQL series in one source package — no `~mysql8.0` suffix)
- **FTP port 21** is often blocked by ISPs/routers; dput supports `method = sftp` (port 22)
- **rapidjson-dev** is required in Build-Depends: MySQL 8.0/8.4 vendor headers pull in `rapidjson/fwd.h`
- **GPGKeyTemporarilyNotFoundError** — normal on first `add-apt-repository` after PPA creation; key appears within 5–30 minutes
- **Result:** `pinba-engine 2.1.2-2~noble1` built and installed from PPA without manual steps

## Commits

- `a14f8ac` — fix(packaging): add rapidjson-dev to Build-Depends

## Links

- Build: https://launchpad.net/~xolegator/+archive/ubuntu/packages/+build/32942019
- PPA: https://launchpad.net/~xolegator/+archive/ubuntu/packages

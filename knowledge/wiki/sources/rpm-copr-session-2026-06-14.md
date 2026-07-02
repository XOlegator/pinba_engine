---
title: "Session: RPM/COPR Packaging (2026-06-14)"
type: source
sources: []
related:
  - wiki/concepts/copr-rpm-packaging.md
  - wiki/concepts/php-extension-packaging.md
  - wiki/concepts/php-version-monitoring.md
confidence: high
updated: 2026-06-14
---

# Source: RPM/COPR Packaging Session — 2026-06-14

A live implementation session that added RPM distribution for the PHP extension
(`github.com/XOlegator/pinba_extension`), the RPM counterpart of the existing Debian/PPA
track. The full concept is in [[copr-rpm-packaging]]; this page records the session's
findings and the work that produced them.

> **Superseded in part (2026-07-02, issue #58).** Remi Collet switched his spec to build
> from this fork, so the extension is now packaged in Remi's own repository for PHP 8.2+
> (modules + SCL). The `php<XY>-php-pinba` SCL subpackages, the per-chroot Remi config, and
> `update-rpm-matrix.php` described below were subsequently **removed** — Copr now builds only
> the distro-native `php-pinba`, and Remi owns the multi-version story. See [[copr-rpm-packaging]]
> for the current model. This page is retained as the record of the original SCL implementation.

## What was built

- `rpm/pinba.spec` (one `php-pinba` SRPM → `php82/83/84/85-php-pinba` SCL subpackages),
  `rpm/build-srpm.sh` (shared build recipe), and `.github/workflows/rpm.yml` (a Fedora
  build gate on push/PR plus a `publish-copr` job that runs `copr-cli build` on a published
  release or manual dispatch).
- `.github/rpm-matrix.json` plus `update-rpm-matrix.php`, wired into the existing weekly
  discovery workflow so one PR refreshes the PHP-support, Debian and RPM matrices.
- Result: a `xolegator/pinba` COPR project publishing 16 packages — `php82..85-php-pinba`
  for Fedora 43/44 and EPEL 9/10 (AlmaLinux/Rocky/RHEL 9/10). Release `v1.3.2` later
  auto-published to both the PPA and COPR from one tag.

PRs: pinba_extension #44 (spec + workflow), #45 (copr-cli SRPM path + config validation),
#47/#49 (README install docs + badge), #48 (matrix + Remi probe), #51 (CodeQL cleanup).

## Key findings

- **COPR is the PPA analog**; EPEL chroots cover the whole RHEL family. One COPR project
  should hold a whole product stack, so the future `pinba-engine` RPM belongs in the same
  `xolegator/pinba` project as a second package.
- **Remi must be configured per chroot, not project-wide.** A project-wide Remi baseurl
  404s on the other distro family (`fedora/9` on EL, `enterprise/44` on Fedora) and DNF
  fails the build hard — reproduced twice, once per direction, before switching to
  per-chroot repos.
- `php<XY>-php-devel` is Remi-only (absent from base Fedora); the spec installs into
  `/opt/remi/php<XY>/...` and the `rpmlint` `dir-or-file-in-opt` finding is expected.
- RPM `%{lua:}` output does not re-expand macros — `%{_libdir}` and `%{?_isa}` must be
  resolved with `rpm.expand()`. Auto debug subpackages must be disabled for the
  build-each-version-in-its-own-tree approach.
- The `COPR_CONFIG` CI secret must contain the full `[copr-cli]` block, not just the token.
- Remi's `primary.xml` metadata is bzip2-compressed; the version-availability probe needs
  a bz2 decoder.
- A successful COPR build leaves only SRPM-phase logs in the build-id directory; the
  binaries and the authoritative package list are in the repo `repodata` (`Packages/p/`),
  which can lag ~1 minute behind a "succeeded" status.

## Also in this session

- License badge switched from the dynamic `github/license` shields endpoint (which
  intermittently rendered "invalid" when shields.io could not reach the GitHub API) to a
  static badge.
- CodeQL `security-and-quality` alerts on the extension were all code-quality findings
  (ambiguously-signed bit fields, guarded `free`, an unused static function, commented-out
  code, an undocumented function) — none were exploitable; resolved in code with no
  behaviour change.

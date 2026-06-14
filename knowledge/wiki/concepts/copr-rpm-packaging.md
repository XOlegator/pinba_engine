---
title: "COPR + Remi RPM Packaging"
type: concept
sources:
  - wiki/sources/rpm-copr-session-2026-06-14.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/github-actions-ppa.md
  - wiki/concepts/launchpad-ppa-workflow.md
  - wiki/concepts/php-extension-packaging.md
  - wiki/concepts/php-version-monitoring.md
confidence: high
updated: 2026-06-14
---

# COPR + Remi RPM Packaging

How the Pinba stack is packaged as `.rpm` for Fedora and Enterprise Linux, the RPM
counterpart of the Debian/Launchpad PPA track (see [[debian-ppa-packaging]],
[[github-actions-ppa]]). First implemented for the PHP extension
(`github.com/XOlegator/pinba_extension`); the same model applies to `pinba_engine`.

## COPR is the RPM analog of a Launchpad PPA

[Fedora COPR](https://copr.fedorainfracloud.org/) is a free Fedora-hosted service that
builds a submitted source RPM in mock chroots and serves the results as a `dnf`
repository — no self-hosted repo or signing infrastructure (COPR signs with its own
per-project key). It is the direct counterpart to a Launchpad PPA.

- **Targets**: Fedora (current N and N-1) plus `epel-9` / `epel-10`. The EPEL chroots
  build against CentOS Stream + EPEL and the resulting packages install on **AlmaLinux,
  Rocky, RHEL and CentOS Stream 9/10**. COPR's UI explicitly recommends the generic
  `epel-N` chroot over distro-specific `almalinux-N` ones.
- **One project hosts many packages.** A single COPR project (e.g. `xolegator/pinba`)
  can hold the whole stack — the `php-pinba` extension package and a future
  `pinba-engine` package — so users enable one repo. This mirrors keeping the whole
  stack in one Launchpad PPA. Group by product/stack, not one project per git repo, and
  not one universal bucket (chroots and external repos are project-wide settings, so an
  unrelated package would inherit them).

## PHP comes from Remi (Software Collections)

The extension subpackages are built against [Remi's](https://rpms.remirepo.net/) PHP
Software Collections, named `php<XY>-php-devel` (`php82-php-devel` … `php85-php-devel`).
Each SCL has its own toolchain and paths:

- `php-config` / `phpize`: `/opt/remi/php<XY>/root/usr/bin/`
- extension dir: `/opt/remi/php<XY>/root/usr/lib64/php/modules/`
- ini scan dir: `/etc/opt/remi/php<XY>/php.d/`

Build per version with `--with-php-config=/opt/remi/php<XY>/root/usr/bin/php-config`.
Runtime packages `Requires: php<XY>-php-common`, which lives in Remi, so **end users must
have Remi enabled** — the RPM equivalent of the `ppa:ondrej/php` runtime dependency in the
Debian track. Remi serves only **Fedora and Enterprise Linux**; it does not cover Mageia,
openSUSE, openEuler, Amazon Linux or Azure Linux, so those COPR chroots cannot build the
package and must not be enabled.

## Critical: configure Remi per chroot, never project-wide

This is the single biggest packaging gotcha. Remi's repository path differs by family:
`rpms.remirepo.net/fedora/$releasever/...` vs `rpms.remirepo.net/enterprise/$releasever/...`.
A single project-wide external-repository baseurl therefore **404s on the other family**:
the `fedora/$releasever` URL becomes `fedora/9` on an EL chroot, and the
`enterprise/$releasever` URL becomes `enterprise/44` on a Fedora chroot. Both DNF4 and
DNF5 treat an unreachable repo's metadata as fatal (there is no effective
`skip_if_unavailable`), so the whole build fails.

The fix: leave the project-wide **External Repositories empty** and add Remi **per chroot**
via each chroot's edit page — the `fedora/...` baseurl on Fedora chroots, the
`enterprise/...` baseurl on EL chroots. Also note `php<XY>-php-devel` is **not** in base
Fedora; it exists only in Remi, so Remi is required on every chroot including Fedora.

## Spec design: one SRPM, many SCL subpackages

A single source package (`php-pinba`) produces one `php<XY>-php-pinba` subpackage per PHP
version, mirroring the Debian multi-binary `php8.X-pinba` scheme and the Remi
`php<XY>-php-pecl-*` convention. Practical spec lessons:

- The version list is one `%global php_versions …` line that the packaging matrix can
  rewrite per target (analog of regenerating `debian/control` Build-Depends per suite).
- Subpackage `%package`/`%files` blocks and per-version `BuildRequires` are generated with
  an RPM `%{lua:}` loop. Macros do **not** re-expand inside Lua output, so resolve
  `%{_libdir}` and `%{?_isa}` with `rpm.expand()` before printing them.
- Disable the auto debug subpackages (`%global debug_package %{nil}`): building each
  version in a separate tree defeats rpm's single-dir debugsource collection.
- A `%check` that loads each freshly built module in its SCL `php` catches breakage in CI.

## Automation from GitHub Actions

`copr-cli build <owner>/<project> <srpm>` submits a build; it is driven from CI with a COPR
API token. The token file (the full `[copr-cli]` section from the COPR `/api/` page) is
stored as a CI secret and written to `~/.config/copr` — storing only the bare token fails
with "File contains no section headers". The workflow builds the SRPM in a Fedora
container, stamps the version from the release tag, and on a published release builds every
target from the matrix file (so a release publishes to all chroots, not just whatever the
project has enabled). This is the same release-triggered, build-logic-in-scripts model as
the [[github-actions-ppa]] track.

## Version tracking mirrors apt probing

A new PHP branch is packaged for a target only once Remi actually ships its
`php<XY>-php-devel` there. The matrix is kept accurate by probing Remi's `primary.xml`
metadata (which is **bzip2**-compressed — the parser needs a bz2 decoder) per target,
resolved to a `fedora` or `enterprise` repo by family + releasever parsed from the chroot
name, and intersecting the result with the upstream-supported branches. This is the RPM
analog of the apt-availability probe in [[php-version-monitoring]] and runs in the same
weekly discovery PR.

## Operational notes

- **Where the binaries are**: a COPR build-id result directory holds only the SRPM-phase
  logs. The built `.rpm`s land in the repository's `Packages/p/` directory; the authoritative
  check that a build published is the repo's `repodata` (primary metadata), not the build-id
  directory. Repo-metadata regeneration can lag ~1 minute after a build reports success.
- **Install** (Fedora or EL with Remi enabled): `dnf copr enable <owner>/<project>` then
  `dnf install php84-php-pinba` (also `php82`/`php83`/`php85`).
- `rpmlint` flags `dir-or-file-in-opt` for these packages — expected and correct, because
  Remi SCL packages legitimately live under `/opt/remi`; it is not gated.

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
updated: 2026-07-02
---

# COPR + Remi RPM Packaging

How the Pinba stack is packaged as `.rpm` for Fedora and Enterprise Linux, the RPM
counterpart of the Debian/Launchpad PPA track (see [[debian-ppa-packaging]],
[[github-actions-ppa]]). First implemented for the PHP extension
(`github.com/XOlegator/pinba_extension`); the same model applies to `pinba_engine`.

## Current model: Copr ships only the distro-native `php-pinba`; Remi owns the rest

Since **2026-07** (issue [XOlegator/pinba_extension#58](https://github.com/XOlegator/pinba_extension/issues/58))
the split of responsibility is:

- **Copr `xolegator/pinba`** builds a single `php-pinba` package against each target's
  **distro-native PHP** (Fedora base repos / EL AppStream). No Remi at build or run time —
  this is the "zero third-party repo" option. `.so` in `/usr/lib64/php/modules`, ini in
  `/etc/php.d/40-pinba.ini`.
- **Remi's own repository** packages **this fork** (upstream since v1.4.0) for its module and
  Software Collection streams — `php-pinba` for a single version and `php<XY>-php-pinba` for
  parallel installs, PHP 8.2+. This is now the canonical source for specific/parallel PHP
  versions on RPM distros. Remi maintainer Remi Collet switched his spec to build from the fork
  and suggested Copr build only for official distro PHP, which is what this track now does.

So the fork's Copr **no longer builds the `php<XY>-php-pinba` SCL subpackages** — they duplicated
what Remi ships. The Remi external repo, the per-chroot Remi config, and the Remi-availability
matrix probe were all removed with the SCL subpackages.

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

## Spec design: one SRPM, one distro-native package

`rpm/pinba.spec` builds `php-pinba` against the chroot's own `php-devel` (`%{_bindir}/phpize`,
`--with-php-config=%{_bindir}/php-config`). A `%check` loads the freshly built `.so` in that
PHP to catch breakage in CI. `%global debug_package %{nil}` disables the debug subpackages —
a single tiny `.so` is not worth a `-debuginfo` pair. On EL the base `php-devel` comes from the
AppStream php module stream (php:8.1 on EL9), which the chroot must have enabled.

`.github/rpm-matrix.json` is now just the Copr target list (`fedora-43/44`, `epel-9/10`) with an
informational `base_php_version` per target; the `publish-copr` job submits `.targets | keys` so a
release builds every target. There is no per-PHP-version list and no auto-refresh: the base build
picks up whatever PHP the chroot ships, and Remi owns the multi-version story.

## Automation from GitHub Actions

`copr-cli build <owner>/<project> <srpm>` submits a build; it is driven from CI with a COPR
API token. The token file (the full `[copr-cli]` section from the COPR `/api/` page) is
stored as a CI secret and written to `~/.config/copr` — storing only the bare token fails
with "File contains no section headers". The workflow builds the SRPM in a stock Fedora
container (no Remi), stamps the version from the release tag, and on a published release builds
every target from the matrix file. This is the same release-triggered, build-logic-in-scripts
model as the [[github-actions-ppa]] track.

## Historical: the SCL subpackage model (2026-06, removed 2026-07)

Before Remi adopted the fork, Copr also built `php<XY>-php-pinba` (php82…php85) against Remi's
PHP Software Collections. The lessons from that model are kept here because they still apply to
any future Remi-SCL packaging:

- SCL toolchain/paths: `php-config`/`phpize` in `/opt/remi/php<XY>/root/usr/bin/`, extension dir
  `/opt/remi/php<XY>/root/usr/lib64/php/modules/`, ini scan dir `/etc/opt/remi/php<XY>/php.d/`.
  `php<XY>-php-devel` is **Remi-only** (absent from base Fedora); runtime `Requires: php<XY>-php-common`.
- **Remi must be configured per chroot, not project-wide.** Remi's path differs by family
  (`rpms.remirepo.net/fedora/$releasever/...` vs `.../enterprise/$releasever/...`); a single
  project-wide baseurl 404s on the other family (`fedora/9` on EL, `enterprise/44` on Fedora) and
  DNF4/DNF5 both treat that as fatal. Fix was per-chroot external repos.
- One SRPM generated a `%package`/`%files` block and per-version `BuildRequires` per PHP via an
  RPM `%{lua:}` loop; macros do **not** re-expand in Lua output, so `%{_libdir}`/`%{?_isa}` were
  resolved with `rpm.expand()`. Each version built in its own tree (hence `debug_package %{nil}`).
- `rpmlint` `dir-or-file-in-opt` was expected because SCL packages live under `/opt/remi`.
- Availability was probed from Remi's `primary.xml` (**bzip2**-compressed) per target and
  intersected with upstream-supported branches, in the weekly discovery PR — the RPM analog of the
  apt probe in [[php-version-monitoring]]. That probe (`update-rpm-matrix.php`) was removed.

## Operational notes

- **Where the binaries are**: a COPR build-id result directory holds only the SRPM-phase
  logs. The built `.rpm`s land in the repository's `Packages/p/` directory; the authoritative
  check that a build published is the repo's `repodata` (primary metadata), not the build-id
  directory. Repo-metadata regeneration can lag ~1 minute after a build reports success.
- **Install the distro-native build** (Fedora or EL, no Remi): `dnf copr enable xolegator/pinba`
  then `dnf install php-pinba`.
- **Install a specific/parallel PHP version**: enable Remi and `dnf install php<XY>-php-pinba`
  (e.g. `php84-php-pinba`) from Remi — see the [Remi wizard](https://rpms.remirepo.net/wizard/).

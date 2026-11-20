---
title: "PHP Extension Debian Packaging"
type: concept
sources:
  - raw/repos/php-pinba-extension.md
related:
  - wiki/sources/pinba-extension-fork.md
  - wiki/concepts/php-extension-build.md
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/launchpad-ppa-workflow.md
  - wiki/concepts/php-version-monitoring.md
confidence: medium
updated: 2026-11-20
---

# PHP Extension Debian Packaging

## Package Model

Source package: **`php-pinba`**

Binary packages: **`php{X.Y}-pinba`** (one per PHP branch)

| Binary Package | Installs |
|---------------|---------|
| `php8.2-pinba` | `pinba.so` for PHP 8.2 |
| `php8.3-pinba` | `pinba.so` for PHP 8.3 |
| `php8.4-pinba` | `pinba.so` for PHP 8.4 |
| `php8.5-pinba` | `pinba.so` for PHP 8.5 |
| `php8.6-pinba` | `pinba.so` for PHP 8.6 |

PPA: `ppa:xolegator/packages`

## Ubuntu Suite Targets

| Suite | Ubuntu Version | PHP Branches |
|-------|---------------|-------------|
| `noble` | 24.04 | 8.2–8.6 (Ubuntu archive and `ondrej/php`) |
| `resolute` | 26.04 | 8.5 (Ubuntu archive) |

Source of truth for the matrix: `.github/packaging-matrix.json`

## Why Versioned Binary Packages

PHP extensions are ABI-specific — the `.so` built for PHP 8.3 will not load under
PHP 8.4. Each PHP version has a different:
- `extension_dir` path (e.g., `/usr/lib/php/20230831/`)
- `API` constant baked into the `.so`
- `php-config8.X` / `phpize8.X` tool chain

Versioned binary packages (`php8.3-pinba`, `php8.4-pinba`) cleanly separate these ABIs
and allow parallel installation of multiple PHP versions on the same host.

## Contrast with MySQL Plugin Packaging

The MySQL plugin (`pinba_engine`) publishes two binary packages from one source package:
`pinba-engine-mysql-8.0` and `pinba-engine-mysql-8.4`. A `debian/pinba-ppa-build.mk` file
inside the source package selects which MySQL variant to build on Launchpad (since
Launchpad can't see the GitHub Actions matrix).

The PHP extension builds one binary package per selected PHP branch.
Launchpad uploads select branches from `.github/packaging-matrix.json` and set
`PHP_VERSIONS` in `debian/php-ppa-build.mk`. The workflow also narrows
`Build-Depends` to the corresponding PHP development packages.

See [[debian-ppa-packaging]] for the MySQL plugin packaging pattern.

## Build and Installation

For each selected branch, `debian/rules` builds the extension with that branch's
`phpize` and `php-config`. It uses `php-config` to locate the extension directory
and generate the `phpapi` dependency. Each binary package includes its own
AppStream metadata and installs `pinba.ini` under
`/etc/php/<version>/mods-available/`. Its post-install script enables the module
with `phpenmod -v <version> pinba`.

Pinba loads when the package enables the module. Set `pinba.enabled=1` and
configure `pinba.server` to send metrics.

## Version Scheme

Follows the same scheme as the MySQL plugin (see [[debian-ppa-packaging]]):

```
{upstream}-{deb_rev}~{suite}{n}
```

Example: `1.2.0-1~noble1`

- `1.2.0` — upstream release
- `1` — Debian revision
- `noble1` — Ubuntu codename + rebuild counter

## Design Constraints

- **No auto-packaging of unvalidated PHP branches.** A newly discovered PHP branch
  (from the weekly monitor) requires CI to pass before packaging automation is enabled.
  See [[php-version-monitoring]].

- **Launchpad uploads from reviewed PRs or release workflows only** — not silent direct pushes.

- **Per-suite source uploads** — same pattern as the MySQL plugin: one `dput` per suite,
  with `-sa` only on the first upload and `-sd` on subsequent ones.

- **The extension must install into the correct `extension_dir`** for each PHP version.
  Launchpad builds in a clean chroot — the correct `php8.X-dev` must be in `Build-Depends`.

## Build Dependencies and CI

The default packaging build uses debhelper, pkgconf, the system `libprotobuf-c-dev`
library and the PHP 8.2–8.6 development packages. Packaging CI builds all five
binary packages on Noble, runs Lintian, and installs and loads the PHP 8.6
package as a smoke test. The release workflow publishes signed source packages
to Launchpad, where binaries are built for each target suite.

See: [[php-extension-build]], [[debian-ppa-packaging]], [[launchpad-ppa-workflow]]

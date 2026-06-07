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
updated: 2026-06-07
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

PPA: `ppa:xolegator/packages`

## Ubuntu Suite Targets

| Suite | Ubuntu Version | PHP Branches |
|-------|---------------|-------------|
| `noble` | 24.04 | 8.2, 8.3, 8.4, 8.5 |
| `resolute` | 26.04 | 8.2, 8.3, 8.4, 8.5 |

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

The PHP extension follows a different strategy:
- One binary package per PHP version
- `dh-php` or per-version `debian/phpX.Y-pinba.install` maps handle multiple packages
- The PHP version list comes from `.github/php-versions.json` (auto-updated by discovery)

See [[debian-ppa-packaging]] for the MySQL plugin packaging pattern.

## Planned debian/ Layout

```
debian/
├── control              — source + binary package declarations
├── rules                — debhelper rules (calls phpize, configure, make per version)
├── changelog            — version history (format: 1.2.0-1~noble1)
├── copyright            — LGPL-2.1 declaration
├── source/format        — "3.0 (quilt)"
├── source/options       — dpkg-source options
├── clean                — generated files to remove between builds
├── php8.2-pinba.install — install map: pinba.so → PHP 8.2 extension_dir
├── php8.3-pinba.install — install map: pinba.so → PHP 8.3 extension_dir
├── php8.4-pinba.install — install map: pinba.so → PHP 8.4 extension_dir
└── php8.5-pinba.install — install map: pinba.so → PHP 8.5 extension_dir
```

## Install Map Pattern

Each `php{X.Y}-pinba.install` file maps the compiled `.so` and the INI snippet:

```
# php8.4-pinba.install
usr/lib/php/20230831/pinba.so
etc/php/8.4/mods-available/pinba.ini
```

The postinst runs `phpenmod 8.4 pinba` (or uses debhelper triggers) to activate the INI.

## Extension INI Template

Installed at `/etc/php/8.4/mods-available/pinba.ini`:

```ini
; Pinba performance monitoring extension
extension=pinba.so
pinba.enabled=0
```

The extension is **disabled by default** — users must set `pinba.enabled=1` and configure
`pinba.server` to enable sending. The INI template is intentionally minimal.

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

## Build-Depends

```
Build-Depends:
  debhelper-compat (= 13),
  dh-php,
  php8.2-dev,
  php8.3-dev,
  php8.4-dev,
  php8.5-dev
```

Unlike the MySQL plugin (which needs `rapidjson-dev`), this extension has no external
library dependencies at compile time — `protobuf-c` is bundled in the source tree.

## Current Status

- `docs/packaging.md` and `.github/packaging-matrix.json` describe the plan
- `debian/` directory is not yet created (next development step)
- CI proves the extension builds and tests pass across all four PHP versions
- Launchpad upload workflow not yet written

See: [[php-extension-build]], [[debian-ppa-packaging]], [[launchpad-ppa-workflow]]

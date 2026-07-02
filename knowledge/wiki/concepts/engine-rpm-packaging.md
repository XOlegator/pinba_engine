---
title: "Engine RPM Packaging (Copr)"
type: concept
sources:
  - wiki/sources/engine-rpm-session-2026-07-02.md
related:
  - wiki/concepts/copr-rpm-packaging.md
  - wiki/concepts/mysql-vendor-headers-minimal.md
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/cmake-build-system.md
confidence: high
updated: 2026-07-02
---

# Engine RPM Packaging (Copr)

How `pinba_engine` is packaged as `.rpm` for Fedora and Enterprise Linux via Copr
`xolegator/pinba` — the RPM counterpart of the Debian/PPA track (see
[[debian-ppa-packaging]]) and the second package in the same Copr project as the
`php-pinba` extension (see [[copr-rpm-packaging]] for the Copr/EPEL fundamentals).
Files live under `rpm/` (`pinba-engine.spec`, `build-srpm.sh`), `.github/workflows/rpm.yml`,
and `.github/rpm-matrix.json`. First published in engine release **v2.7.0**; MariaDB flavor
in **v2.8.x**, Fedora 43 in **v2.9.0**, aarch64 in **v2.10.0**.

## The defining difference: offline build against vendored *server* headers

Unlike the PHP extension (which builds against the distro's `php-devel`), the engine is a
**storage-engine plugin** compiled against the database server's internal *source* headers
(`sql/handler.h`, `field.h`, `table.h` …), which **no `-devel` package ships**. Those headers
are vendored in-tree — `vendor/mysql-headers-8.x` (see [[mysql-vendor-headers-minimal]]) and
`vendor/mariadb-headers-{10.11,11.8}` — and travel inside the source tarball, so the RPM builds
**fully offline in mock/Copr**: no server package, no network, no CMake configure of the DB
server at package-build time. `%check` is a symbol check (`nm -D … | grep -q _mysql_plugin_declarations_`
or `_maria_plugin_declarations_`), not a live `INSTALL PLUGIN` (there is no running server in mock).

## Two flavors, one per build, mutually exclusive

MySQL and MariaDB are different servers; a plugin built for one will not load in the other,
and both install the **same** `ha_pinba.so` path. So:

- The spec is **flavor-parameterized** (`%{flavor}` = `mysql` | `mariadb`); **one flavor per
  build**. `rpm/build-srpm.sh <version> <flavor>` bakes the flavor into the SRPM (a leading
  `%global flavor …`) so a Copr rebuild produces the right one.
- Binary packages: **`pinba-engine-common`** (noarch — the report-table schema + docs) and
  **`pinba-engine-<flavor>`** (`ha_pinba.so` in `%{_libdir}/mysql/plugin`, `Requires:` the
  matching server, `Conflicts:` the other flavor — one server per host).
- `publish-copr` submits **one SRPM per flavor**; each Copr build yields that flavor's package
  for its chroots.

## ABI: vendored header series must match the chroot's server

The plugin is ABI-bound to the server's **minor** version (see [[mysql-plugin-abi]]), so the
spec picks the vendored series from the build macros:

- **MariaDB**: `%{?fedora} >= 44` → 11.8; else (Fedora 43 and EL9/EL10) → 10.11.
  (Verified: Fedora 43 ships MariaDB 10.11, Fedora 44 ships 11.8, EL9/EL10 ship 10.11.)
- **MySQL**: 8.4 everywhere (Fedora and current EL both ship 8.4; EL9's `mysql` module default
  stream moved 8.0 → 8.4). Only the `mysql-devel` build header comes from a package; the server
  *source* headers are vendored.

`.github/rpm-matrix.json` lists each target's `flavors` and series; the publish job routes each
flavor only to the chroots that list it.

## MySQL-on-EL is deferred; no `module_enable` on EPEL chroots

The **MariaDB flavor is primary** (default DB on Fedora/EL) and builds on every chroot. The
**MySQL flavor is Fedora-only for now**: on EL9 `mysql` is a **module** (`mysql:8.4`) and
`mysql-devel` lives behind it, so MySQL-on-EL needs extra chroot setup.

Critically, **the EPEL chroots need no `module_enable` for these builds** — the MariaDB flavor
pulls no DB packages at build time (vendored headers). Enabling a **non-existent** module aborts
*every* build in the chroot: the first live publish failed because `epel-9` had
`module_enable = mariadb:10.11,mysql:8.0`, and `dnf module enable … mysql:8.0` errored with
`missing groups or modules: mysql:8.0` (there is no `mysql:8.0` module on EL9), taking the
MariaDB build down with it. Fix: clear the EPEL module list.

## Vendoring MariaDB headers (`scripts/extract-mariadb-headers.sh`)

`scripts/prepare-mariadb-headers.sh` runs just enough of MariaDB's own CMake (`GenError`) to
produce the generated headers — no full server build. `scripts/extract-mariadb-headers.sh` then
derives the **minimal** subset from the compiler `.d` dependency files of a real build and
**validates** it by rebuilding the plugin against only the subset (~130 files, ~2.6 MB per series,
like the MySQL approach in [[mysql-vendor-headers-minimal]]). Two non-obvious rules:

- **Generated headers must be committed** for MariaDB. `my_config.h`, `mysql_version.h`
  (with `MARIADB_BASE_VERSION`), `mysqld_error.h`, … normally live under `builddir/` (gitignored,
  because pinba's CMake *synthesises* `mysql_version.h` for MySQL). pinba's CMake **never
  synthesises** them for MariaDB, so a `.gitignore` exception carves
  `vendor/mariadb-headers-*/builddir/` back in.
- **CMake 4 (Fedora 44+)** rejects the `cmake_minimum_required(<3.5)` in MariaDB 10.11's bundled
  `columnstore`; the header-gen configure passes `-DCMAKE_POLICY_VERSION_MINIMUM=3.5
  -DPLUGIN_COLUMNSTORE=NO`.

## aarch64 support

ARM servers (AWS Graviton, Ampere, Oracle Cloud ARM) run MySQL/MariaDB, and Copr has native
aarch64 builders. Findings when enabling `*-aarch64`:

- `my_config.h` is **arch-independent for our targets** — all builds are 64-bit little-endian, so
  `SIZEOF_*`, `STACK_DIRECTION`, endianness are identical; the only x86-ism is the cosmetic
  `MACHINE_TYPE "x86_64"` string, which the plugin does not use.
- The real gap was the **minimal header subset being x86_64-path-specific**: `my_byteorder.h`
  includes `byte_order_generic_x86_64.h` on x86_64 but `byte_order_generic.h` on every other arch,
  and the `.d` harvest (run on x86_64) never captured the latter → `fatal error:
  byte_order_generic.h: No such file or directory` on ARM. Fix: force-include that one header in
  the vendored subset.
- The **MySQL flavor needed no change** — MySQL 8.x uses a portable byteorder (no per-arch
  `byte_order_generic` split).

## Matrix, routing and version

`.github/rpm-matrix.json` targets (v2.10.0): **MariaDB** → all 8 chroots (`fedora-43/44`,
`epel-9/10`, each × `x86_64`+`aarch64`); **MySQL** → the 4 Fedora chroots. The gate in
`rpm.yml` matrices over `fedora:43` + `fedora:44` (x86_64) so both vendored MariaDB series
(10.11 and 11.8) are exercised in CI. The spec `Version` is **release-please-managed** via
`x-release-please-start/end-version` block markers (`build-srpm.sh` still stamps the tag version
at release time).

## Install

```
dnf copr enable xolegator/pinba
dnf install pinba-engine-mariadb   # or pinba-engine-mysql on Fedora
```

Then `INSTALL PLUGIN pinba SONAME 'ha_pinba.so'` and load
`%{_datadir}/pinba_engine/default_tables.sql`. See [[copr-rpm-packaging]] for Copr operational
notes (where binaries land, `repodata` lag, the `COPR_CONFIG` secret format).

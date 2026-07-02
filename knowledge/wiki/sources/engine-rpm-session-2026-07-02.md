---
title: "Session: Engine RPM Packaging (2026-07-02)"
type: source
sources: []
related:
  - wiki/concepts/engine-rpm-packaging.md
  - wiki/concepts/copr-rpm-packaging.md
  - wiki/concepts/mysql-vendor-headers-minimal.md
  - wiki/concepts/mysql-plugin-abi.md
confidence: high
updated: 2026-07-02
---

# Source: Engine RPM Packaging Session — 2026-07-02

A live implementation session that added `.rpm` distribution for `pinba_engine` via Copr
`xolegator/pinba` (the second package alongside `php-pinba`), the RPM counterpart of the
engine's Debian/PPA track. The full concept is in [[engine-rpm-packaging]]; this page records
the session's findings and the work that produced them.

## What was built

- `rpm/pinba-engine.spec` (flavor-parameterized: `pinba-engine-common` + `pinba-engine-mysql` /
  `pinba-engine-mariadb`), `rpm/build-srpm.sh` (bakes the flavor into the SRPM),
  `.github/workflows/rpm.yml` (Fedora gate on push/PR + release-triggered `publish-copr`), and
  `.github/rpm-matrix.json` (per-flavor, per-chroot targets).
- `scripts/extract-mariadb-headers.sh` — derives the minimal MariaDB header subset from `.d`
  files and validates by rebuilding against it; produced `vendor/mariadb-headers-{10.11,11.8}`.
- Result: `pinba-engine-mariadb` on Fedora 43/44 + EL9/EL10 and `pinba-engine-mysql` on Fedora,
  both `x86_64` and `aarch64` — 8 mariadb chroots + 4 mysql chroots, all green at v2.10.0.

Releases: v2.7.0 (MySQL flavor foundation), v2.8.x (MariaDB flavor + Copr publish), v2.9.0
(Fedora 43 + release-please-managed spec Version), v2.10.0 (aarch64). PRs: #58, #60, #62, #65.

## Key findings

- **Offline build against vendored server headers** is the whole game: the engine compiles
  against `sql/handler.h` etc. which no `-devel` ships, so the minimal header subset is committed
  and travels in the tarball → mock builds with no network/server. `%check` is a `nm` symbol
  check, not a live `INSTALL PLUGIN`.
- **One flavor per SRPM, baked in.** MySQL and MariaDB plugins share the `ha_pinba.so` path and
  can't coexist in one buildroot; the flavor is a `%global` injected by `build-srpm.sh` so Copr
  rebuilds the right one. The two flavor packages `Conflicts` each other.
- **ABI: series per chroot.** Fedora 44 → MariaDB 11.8; **Fedora 43 → MariaDB 10.11** (verified
  10.11.18, not 11.8); EL9/EL10 → 10.11. MySQL 8.4 everywhere. The spec selects via `%{?fedora}`.
- **EPEL `module_enable` must not name a non-existent module.** The first live publish failed on
  `epel-9` because the chroot enabled `mysql:8.0`, which does not exist on EL9 (it's `mysql:8.4`,
  behind a module); the failing `dnf module enable` aborted the whole chroot, including the MariaDB
  flavor that needs no module at all. Fix: clear the EPEL module list. MySQL-on-EL is deferred.
- **MariaDB generated headers must be committed** (`.gitignore` exception on
  `vendor/mariadb-headers-*/builddir/`): pinba's CMake synthesises `mysql_version.h` for MySQL but
  uses MariaDB's real generated `my_config.h` / `mysql_version.h` (with `MARIADB_BASE_VERSION`).
- **aarch64**: `my_config.h` is arch-independent for 64-bit LE (only the cosmetic `MACHINE_TYPE`
  differs). The real ARM failure was a missing `byte_order_generic.h` — the vendored subset was
  x86_64-path-specific (the `.d` harvest ran on x86_64, where `my_byteorder.h` takes the
  `byte_order_generic_x86_64.h` branch). Force-including the generic header fixed it. MySQL's
  byteorder is portable, so its flavor built on ARM unchanged.
- **CMake 4 vs old MariaDB**: Fedora 44 ships CMake 4.3, which rejects
  `cmake_minimum_required(<3.5)` in MariaDB 10.11's bundled `columnstore` during header-gen; the
  prepare script passes `-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DPLUGIN_COLUMNSTORE=NO`.
- **Copr operational** (shared with [[copr-rpm-packaging]]): `copr-cli build` waits and returns
  non-zero on any chroot failure; the `COPR_CONFIG` secret must be the full `[copr-cli]` block;
  a transient Launchpad-style queue delay is normal before a build leaves `starting`.

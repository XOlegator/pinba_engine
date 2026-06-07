---
title: "Debian/PPA Packaging for MySQL Storage Engine Plugins"
type: concept
sources:
  - raw/sessions/ppa-packaging-session-2026-06-04.md
  - raw/sessions/ppa-upload-session-2026-06-06.md
related:
  - wiki/concepts/mysql-postinst-patterns.md
  - wiki/concepts/mysql-vendor-headers-minimal.md
  - wiki/concepts/mysql-plugin-install.md
  - wiki/concepts/cmake-build-system.md
  - wiki/concepts/launchpad-ppa-workflow.md
confidence: high
updated: 2026-06-07
---

# Debian/PPA Packaging for MySQL Storage Engine Plugins

How to build a `.deb` package for a MySQL storage engine plugin and publish it to
a Launchpad PPA. Using `pinba_engine` as the example.

## debian/ structure

```
debian/
├── control           — Source + Binary packages, Build-Depends
├── rules             — cmake configuration per MySQL series
├── changelog         — package version, distribution (never UNRELEASED for PPA uploads)
├── copyright         — DEP-5 format
├── source/format     — "3.0 (quilt)"
├── pinba-engine-common.install         — SQL files to /usr/share/pinba_engine/
├── pinba-engine-mysql-8.0.install      — ha_pinba.so to /usr/lib/mysql/plugin/
├── pinba-engine-mysql-8.4.install
├── pinba-engine-mysql-8.0.postinst     — plugin install + database creation
├── pinba-engine-mysql-8.0.prerm        — plugin unload
├── pinba-engine-mysql-8.4.postinst
└── pinba-engine-mysql-8.4.prerm
```

## Version format

```
{upstream_version}-{deb_revision}~{ubuntu_codename}{n}

Examples:
  2.3.0-1~noble1      (Noble, any MySQL series — series is selected by debian/pinba-ppa-build.mk)
  2.3.0-1~resolute1   (Resolute)
  2.3.0-2~noble1      (Noble, second upload of same upstream after packaging fix)
```

The `~` operator sorts lower than any suffix: `2.3.0~... < 2.3.0` — Ubuntu sees upstream
as newer than the PPA package, which is the correct ordering.

One source package builds both `pinba-engine-mysql-8.0` and `pinba-engine-mysql-8.4` binaries,
so no `~mysql8.0` suffix is needed in the version string.

In the automated flow, `deb_revision` is stored per suite in `.github/mysql-versions.json`
under the `debian_revision` key and is incremented automatically when a new MySQL patch
version is detected.

## debian/rules

The source package can build both binary packages, but the actual set is controlled by the
generated file `debian/pinba-ppa-build.mk`:

```makefile
-include debian/pinba-ppa-build.mk
ENABLE_MYSQL80 ?= 1
ENABLE_MYSQL84 ?= 1

ifeq ($(ENABLE_MYSQL80),0)
    DH_EXCLUDE_PACKAGES += -Npinba-engine-mysql-8.0
endif
```

This allows GitHub Actions to generate different source uploads for:
- `noble` → only `pinba-engine-mysql-8.0`
- `resolute` → only `pinba-engine-mysql-8.4`

The main CMake configuration in `debian/rules`:

```makefile
override_dh_auto_configure:
    cmake -B debian/build-mysql80 \
      -DPINBA_MYSQL_SERIES=8.0 \
      -DPINBA_MYSQL_SOURCE_DIR=$(CURDIR)/vendor/mysql-headers-8.0 \
      -DPINBA_MYSQL_SOURCE_VERSION=8.0.46 \
      -DPINBA_WITH_TESTS=OFF \
      -DPINBA_FETCH_DEPENDENCIES=OFF \
      -DCMAKE_INSTALL_LIBDIR=lib
    cmake -B debian/build-mysql84 ...

override_dh_auto_build:
    cmake --build debian/build-mysql80 --parallel $(NPROC)
    cmake --build debian/build-mysql84 --parallel $(NPROC)

override_dh_auto_install:
    DESTDIR=$(CURDIR)/debian/tmp-mysql80 cmake --install debian/build-mysql80
    DESTDIR=$(CURDIR)/debian/tmp-mysql84 cmake --install debian/build-mysql84

override_dh_install:
    dh_install --sourcedir=debian/tmp-mysql80 -ppinba-engine-mysql-8.0
    dh_install --sourcedir=debian/tmp-mysql84 -ppinba-engine-mysql-8.4
    dh_install --sourcedir=debian/tmp-mysql80 -ppinba-engine-common
```

Note: the MySQL version numbers in `debian/rules` are overridden at build time by
values read from `.github/mysql-versions.json` (via the `ppa-build.yml` workflow).
`debian/rules` contains defaults; the CI always passes the correct versions explicitly.

## Vendoring MySQL headers

MySQL plugin API requires `sql/handler.h` — a header from the full MySQL source tree,
not available in `libmysqlclient-dev`. Launchpad builds in an isolated network-less chroot.

**Solution:** vendor only the headers actually needed (162/175 files, ~2.2 MB per series)
in `vendor/mysql-headers-{series}/` and commit them to git.

Details: [[mysql-vendor-headers-minimal]].

## Building and testing locally

```bash
# Build binary packages (no signing):
dpkg-buildpackage -us -uc -b

# Check lintian:
lintian --fail-on error pinba-engine-mysql-8.0_*.deb

# Install:
sudo dpkg -i pinba-engine-common_*.deb pinba-engine-mysql-8.0_*.deb
```

Expected lintian warnings (non-critical):
- `initial-upload-closes-no-bugs` — normal for PPA uploads, ITP not required
- `debian-changelog-has-wrong-day-of-week` — verify the day of week manually

## Creating the orig.tar.gz and source package

`dpkg-buildpackage -S -sa` does **not** create the `orig.tar.gz` itself — it expects
`../pinba-engine_{VERSION}.orig.tar.gz` to already exist.
Create it with `git archive` before each PPA upload:

```bash
git archive --prefix=pinba-engine-2.3.0/ HEAD | gzip -9 > ../pinba-engine_2.3.0.orig.tar.gz
dpkg-buildpackage -S -sa -us -uc
```

`vendor/mysql-headers-*` are committed to git → they are included in the orig automatically.
Final orig size: ~1.1 MB.

Two files are required for a correct source build:
- `debian/clean` — list of cmake build trees to delete (otherwise "unwanted binary" error)
- `debian/source/options` with `--extend-diff-ignore` — excludes local untracked files

## Uploading to Launchpad (dput)

In the automated GitHub Actions flow, uploads use plain FTP with `passive_ftp = 1`.
GitHub Actions runners are behind NAT; passive mode is required for FTP data connections
to work through NAT. See [[github-actions-ppa]] for the full dput configuration.

For local manual uploads, SFTP is recommended (FTP port 21 is often blocked by ISPs):

```ini
# ~/.dput.cf
[xolegator-packages]
fqdn            = ppa.launchpad.net
method          = sftp
incoming        = ~xolegator/packages/ubuntu/
login           = xolegator
allow_unsigned_uploads = 0
```

Signing and uploading:
```bash
debsign -k{FINGERPRINT} ../pinba-engine_*_source.changes
dput xolegator-packages ../pinba-engine_*_source.changes
```

`debsign` requires an interactive terminal for GPG pinentry. Do not run it inside a
non-TTY script.

## rapidjson-dev is required in Build-Depends

MySQL 8.0 and 8.4 vendor headers pull in `<rapidjson/fwd.h>` through the chain:

```
ha_pinba.cc → sql/table.h → sql/dd/types/foreign_key.h → sql/dd/sdi_fwd.h → rapidjson/fwd.h
```

This file is not in `vendor/mysql-headers-*` (it lives under `extra/rapidjson/` in the full
MySQL source). Local builds may succeed with a stale cmake cache from a full MySQL source.
Launchpad builds in a clean chroot and fail immediately.

**Fix:** `rapidjson-dev` in `debian/control` Build-Depends.
On Ubuntu Noble: `rapidjson-dev 1.1.0+dfsg2-7.2`, available in the `universe` component.

## Installing from PPA — notes

After the first PPA publish, `add-apt-repository` may return `GPGKeyTemporarilyNotFoundError`
(Launchpad HTTP 500). This is temporary — the PPA signing key is generated with a delay of
5–30 minutes. Wait and retry.

If needed before the key propagates:
```bash
echo "deb https://ppa.launchpadcontent.net/xolegator/packages/ubuntu/ noble main" \
  | sudo tee /etc/apt/sources.list.d/xolegator-packages.list
sudo gpg --keyserver keyserver.ubuntu.com --recv-keys {FINGERPRINT}
sudo gpg --export {FINGERPRINT} | sudo tee /etc/apt/trusted.gpg.d/xolegator-packages.gpg > /dev/null
```

After a successful `add-apt-repository`, remove the manually added `.list` file to avoid
duplicate sources.

## postinst/prerm details

Full reference: [[mysql-postinst-patterns]].

Summary:
1. postinst: writes `/etc/mysql/conf.d/pinba-engine.cnf` (`plugin-load-add`)
2. postinst: attempts `INSTALL PLUGIN` for immediate activation (may fail, non-fatal)
3. postinst: creates database `pinba` and imports `default_tables.sql` (only if plugin is active)
4. prerm: removes config, unloads plugin if loaded

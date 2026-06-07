---
title: "Activity Log"
type: log
sources: []
related:
  - wiki/index.md
confidence: high
updated: 2026-06-07
---

# Activity Log

Chronological record of all ingest, query, lint, and revision operations.

---

## 2026-06-07 — REVISION: Full wiki audit and translation

**Action:** Full revision of the knowledge base per the Karpathy LLM Wiki methodology:
stale facts identified against the actual codebase state, Russian content translated to
English, English-only rule codified in SCHEMA.md and AGENTS.md.

**Stale facts corrected:**

- `github-actions-mysql-version-monitor.md`: claimed the monitor "opens an issue".
  Actual current behaviour (since commit `c6dc449` / v2.4.0): creates a PR with
  auto-merge enabled, triggering a full automated release cycle on merge.
  Also removed hardcoded MySQL version numbers — the tracked versions live in
  `.github/mysql-versions.json` and change over time.
- `github-actions-ppa.md`: stated upload transport is SFTP. Actual transport since
  fix #34: plain FTP with `passive_ftp = 1` and a 3-attempt retry loop.
  Also missing: `ppa-build` triggers on push to `.github/mysql-versions.json`.
  Also missing: `debian_revision` is now read from `mysql-versions.json`.
  Updated version example from `2.1.2` to `2.3.0` (current is `2.4.0`).
- `debian-ppa-packaging.md`: stated FTP port 21 is blocked and recommended SFTP
  as the primary transport for CI. Actual CI transport is FTP with `passive_ftp = 1`.
  Also had wrong version scheme description (showed `~ubuntu24.04~mysql8.0` format,
  actual format is `{upstream}-{deb_rev}~{codename}{n}`).
- `launchpad-ppa-workflow.md`: entirely in Russian; also recommended SFTP for all use
  cases without distinguishing local vs CI context.
- `docker-tag-strategy.md`: "Future: GitHub Actions CI" section described planned work
  that is already implemented. Replaced with present-tense description.
- `wiki/index.md`: `launchpad-ppa-workflow` table row contained Russian text.
- `wiki/overview.md`: "Tag cleanup in progress" note is stale; project status section
  did not mention the automated release cycle. Version was not mentioned.

**Russian content translated:**
- `wiki/concepts/launchpad-ppa-workflow.md` — fully rewritten in English
- `wiki/concepts/debian-ppa-packaging.md` — fully rewritten in English
- `wiki/concepts/mysql-postinst-patterns.md` — fully rewritten in English
- `wiki/concepts/mysql-vendor-headers-minimal.md` — fully rewritten in English
- `wiki/log.md` entries from 2026-06-04 and 2026-06-06 — translated in place
- `wiki/sources/ppa-packaging-session-2026-06-04.md` — translated
- `wiki/sources/ppa-upload-session-2026-06-06.md` — translated

**Rules added:**
- `knowledge/SCHEMA.md`: Language Policy section (English only) and Revision/Audit
  Procedure section with concrete steps.
- `AGENTS.md`: knowledge base English-only rule added alongside the existing commit
  message English rule.

**Files updated:** SCHEMA.md, AGENTS.md, wiki/index.md, wiki/overview.md, wiki/log.md,
wiki/concepts/github-actions-mysql-version-monitor.md, wiki/concepts/github-actions-ppa.md,
wiki/concepts/launchpad-ppa-workflow.md, wiki/concepts/debian-ppa-packaging.md,
wiki/concepts/mysql-postinst-patterns.md, wiki/concepts/mysql-vendor-headers-minimal.md,
wiki/concepts/docker-tag-strategy.md, wiki/sources/ppa-packaging-session-2026-06-04.md,
wiki/sources/ppa-upload-session-2026-06-06.md.

---

## 2026-06-07 — Workflow Fix: Launchpad FTP upload 550 error — passive mode

**Symptom:** `upload-to-ppa` job: noble upload succeeds, resolute upload fails with
`550 Requested action not taken: internal server error`. dput prints:
`Note: This error might indicate a problem with your passive_ftp setting.`

**Root cause:** GitHub Actions runners are behind NAT. Active FTP mode requires
Launchpad's server to open a data connection back to the runner, which NAT blocks.
This is a different issue from the earlier orig-tarball conflict (fixed by `-sd`).

**Fix applied in #34:**
- `passive_ftp = 1` in dput config — client initiates data connection, works through NAT
- retry loop (3 attempts, 30 s gap) — resilience against transient Launchpad FTP errors

**Distinguishing from prior issue (noble OK / resolute fails):**
- Prior (2026-06-06): resolute failed because orig.tar.gz was uploaded twice → fixed by `-sd`
- Current (2026-06-07): resolute failed with `550` on `.dsc` upload → FTP passive mode

---

## 2026-05-23 — Initial Setup + First Ingest

**Action:** Knowledge base structure created. First batch ingest of 7 raw documents.

**Raw documents added:**
- `raw/repos/tony2001-pinba-engine-readme.md` — original README
- `raw/repos/tony2001-pinba-engine-changelog.md` — version history 0.0.3–1.2.0
- `raw/repos/php-pinba-extension.md` — PHP extension docs, INI, functions
- `raw/docs/pinba-udp-protocol-proto.md` — full protobuf spec with field explanations
- `raw/docs/pinba-engine-mysql-sysvars.md` — all MySQL system variables + plugin declaration
- `raw/docs/pinba-default-tables.md` — complete schema of all 18+ pinba tables
- `raw/docs/mysql-plugin-api-install.md` — MySQL plugin API, INSTALL PLUGIN, ABI notes
- `raw/docs/xolegator-fork-build-docker.md` — fork build system, Docker architecture

**Source pages created (wiki/sources/):**
- `tony2001-readme.md`
- `php-pinba-extension.md`
- `pinba-udp-protocol.md`
- `mysql-plugin-api.md`
- `pinba-engine-sysvars-and-tables.md`
- `fork-build-docker.md`

**Concept pages created (wiki/concepts/):**
- `pinba-data-flow.md` — end-to-end pipeline, silent failure table
- `pinba-udp-protocol.md` — transport, timer encoding, field versions
- `pinba-pool-model.md` — cyclic buffers, overflow, monitoring queries
- `mysql-plugin-abi.md` — **8.0 vs 8.4 incompatibility** (most critical concept)
- `mysql-plugin-install.md` — lifecycle, reinstall procedure
- `php-pinba-configuration.md` — deployment scenarios, verification checklist
- `cmake-build-system.md` — presets, header sourcing, Ubuntu 24.04 deps
- `docker-build-strategy.md` — multi-stage build, libprotobuf copy, filename gotchas
- `docker-tag-strategy.md` — naming rules, rolling vs pinned tags

**Updated:** index.md (full catalog), overview.md (high-level synthesis).

**Notes:** tony2001-pinba-engine-changelog.md was not given its own source page
(content folded into overview and concepts). MySQL Storage engine types article
from dev.mysql.com was not separately stored (generic MySQL content, not Pinba-specific).

**Knowledge gaps identified** (candidates for next ingest):
- MySQL 8.0→8.4 API delta (exact changed structures)
- Pinboard architecture details
- CI/CD GitHub Actions for Docker push automation
- Production deployment docker-compose
- PHP site integration examples

---

## 2026-05-23 — Bug Fix: MySQL Source / Runtime Version Sync

**Action:** Fixed plugin ABI mismatch between MySQL 8.0.45 source headers and MySQL 8.0.46 runtime.

**Problem:** `CMakeLists.txt` downloaded MySQL `8.0.45` source headers to compile the plugin,
but `Dockerfile.mysql80` pointed to `mysql:8.0-bookworm` which is currently MySQL `8.0.46`.
`MYSQL_HANDLERTON_INTERFACE_VERSION` changed between patch versions, causing:
```
ERROR 1126 (HY000): Can't open shared library 'libpinba_engine.so'
  (errno: 0 API version for STORAGE ENGINE plugin is too different)
```

**Fixes applied:**
- `CMakeLists.txt`: bumped `PINBA_DEFAULT_MYSQL_SOURCE_VERSION` from `8.0.45` → `8.0.46`,
  updated `PINBA_DEFAULT_MYSQL_SOURCE_HASH` to `MD5=6707beb0d46a9e08a19aa596329ca79d`
- `Dockerfile.mysql80`: switched from floating `mysql:8.0-bookworm@sha256:...` to explicit
  `mysql:8.0.46-bookworm` tag — eliminates future silent version drift
- `Dockerfile.mysql84`: switched from floating `mysql:8.4` to explicit `mysql:8.4.9` tag
- `wiki/concepts/mysql-plugin-abi.md`: documented that minor-version mismatch (8.0.45 vs 8.0.46)
  causes the same ABI error as major-version mismatch

**Lesson:** The source version in `CMakeLists.txt` and the runtime tag in the Dockerfile
**must be kept in lockstep**. Floating runtime tags (`mysql:8.0`) will silently drift past the
pinned source version on the next Docker pull.

---

## 2026-05-23 — Root Cause Found: MYSQL_VERSION_ID Never Defined (mysql_version.h.in only)

**Action:** Fixed the fundamental root cause of all ABI mismatches.

**Problem:** `MYSQL_HANDLERTON_INTERFACE_VERSION` is defined as `(MYSQL_VERSION_ID << 8)` in
`plugin.h`. The MySQL source tarball only ships `include/mysql_version.h.in` (a CMake template),
NOT the generated `include/mysql_version.h`. Without running `cmake --configure` on the MySQL
source, `MYSQL_VERSION_ID` is never defined. The compiler treats undefined macros as `0`, so:
```
MYSQL_HANDLERTON_INTERFACE_VERSION = (0 << 8) = 0
```
The runtime server has e.g. `MYSQL_HANDLERTON_INTERFACE_VERSION = (80046 << 8)`. The check:
```c
if ((info->interface_version >> 8) != (MYSQL_HANDLERTON_INTERFACE_VERSION >> 8))
    // "API version for STORAGE ENGINE plugin is too different"
```
fails for EVERY MySQL version because 0 never matches any real version.

Additionally: `ha_pinba.cc:31` has `#include <mysql/mysql_version.h>` which finds the
MariaDB-compat version from debian:bookworm's `default-libmysqlclient-dev` package.
MariaDB's version sets a wrong `MYSQL_VERSION_ID` (e.g. MariaDB 10.x version number).

**Fix in `CMakeLists.txt`:**
1. Compute `MYSQL_VERSION_ID` from `PINBA_MYSQL_SOURCE_VERSION` (e.g. `8.0.46` → `80046`)
2. Generate `builddir/include/mysql_version.h` AND `builddir/include/mysql/mysql_version.h`
   with the correct `MYSQL_VERSION_ID`
3. `${PINBA_MYSQL_BUILDDIR_INCLUDE}` is in the include path before `${MYSQL_INCLUDE_DIR}`,
   so the generated file takes precedence over MariaDB's system header

---

## 2026-05-23 — MySQL 8.4 Runtime is Oracle Linux 9, Not Debian

**Action:** Fixed MySQL 8.4 Docker image — library path and OS mismatch.

**Problem:** `mysql:8.4.9` Docker image is based on **Oracle Linux 9.7** (RPM layout),
NOT Debian bookworm (DEB layout). The two OS families use different paths:
- Debian: `/usr/lib/x86_64-linux-gnu/` for libraries
- OL9: `/usr/lib64/` for libraries, `/usr/lib64/mysql/plugin/` for MySQL plugins

`Dockerfile.mysql84` was copying `libprotobuf.so.*` to the wrong Debian path.
The plugin itself was being placed at the wrong path too.

**Compatibility analysis:**
- glibc: builder has 2.36, OL9 has 2.34. Plugin uses GLIBC_2.34 max → compatible
- libstdc++: plugin uses GLIBCXX_3.4.29 max. OL9's GCC 11 provides 3.4.29 → compatible
- Only `libprotobuf.so.32` was missing on OL9

**Fixes in `Dockerfile.mysql84`:**
- Plugin copy destination: `/usr/lib64/mysql/plugin/libpinba_engine.so`
- libprotobuf copy destination: `/usr/lib64/`
- Added `ldconfig` after COPY to register the new library

---

## 2026-05-23 — All Docker Hub Tags Published

**Action:** Built, validated, and pushed all 5 tags to Docker Hub.

| Tag | Image | SHA256 |
|-----|-------|--------|
| `8.0`, `latest`, `8.0-v2.0.0` | MySQL 8.0.46 / Debian bookworm | `d24533be...` |
| `8.4-lts`, `8.4-lts-v2.0.0` | MySQL 8.4.9 / Oracle Linux 9.7 | `f888c00e...` |

Both containers validated:
- `SHOW PLUGINS;` → `PINBA ACTIVE STORAGE ENGINE libpinba_engine.so GPL`
- `SHOW TABLES;` → all 25 pinba tables present

---

## 2026-05-23 — Ingest: Pinboard Architecture

**Action:** Explored `/mnt/projects/sites/pinba/www-new/` and compiled architecture doc.

**Raw document added:**
- `raw/repos/pinboard-architecture.md` — Symfony 8 stack, aggregate command logic, Docker Compose services, env vars

**Concept page created:**
- `wiki/concepts/pinboard-architecture.md` — full aggregation pipeline, source vs report table distinction, docker compose, env config reference

---

## 2026-05-23 — Ingest: MySQL 8.0→8.4 Migration Guide

**Action:** Researched official MySQL documentation on 8.0→8.4 plugin API changes.

**Raw document added:**
- `raw/docs/mysql-80-to-84-migration.md` — ABI incompatibility, deprecated handler methods, build requirements, OS runtime differences

**Concept page created:**
- `wiki/concepts/mysql-plugin-migration-80-to-84.md` — what changed, what stayed the same, porting checklist

---

## 2026-05-23 — Ingest: GitHub Actions Docker Workflow

**Action:** Researched canonical GitHub Actions workflow for Docker Hub build+push.

**Raw document added:**
- `raw/docs/github-actions-docker-workflow.md` — canonical action versions, full workflow YAML, design rationale

**Concept page created:**
- `wiki/concepts/github-actions-docker.md` — project-specific workflow (two jobs for 8.0 and 8.4 LTS, correct tag names, GHA cache scopes)

---

## 2026-06-04 — Ingest: Debian PPA Packaging Session

**Action:** Development and debugging session for Debian/PPA packaging of `pinba_engine`.
Closed Stage 1.5 and Stage 2 of the internal PPA automation plan.

**Raw document added:**
- `raw/sessions/ppa-packaging-session-2026-06-04.md` — session log with diagnostics

**Source page created:**
- `wiki/sources/ppa-packaging-session-2026-06-04.md`

**Concept pages created:**
- `wiki/concepts/debian-ppa-packaging.md` — full PPA packaging process for a MySQL plugin:
  `debian/` structure, versioning scheme, header vendoring, lintian, Launchpad
- `wiki/concepts/mysql-postinst-patterns.md` — postinst/prerm patterns and pitfalls:
  authentication cascade (`debian.cnf` → `auth_socket`), `plugin-load-add` vs `INSTALL PLUGIN`,
  MySQL 8.0 vs 9.0 syntax pitfalls, `DROP DATABASE` with an unloaded ENGINE
- `wiki/concepts/mysql-vendor-headers-minimal.md` — minimal vendor headers strategy:
  compiler `.d` file analysis, whitelist extraction algorithm, result 1317 → 162 files

**Key findings documented:**
- `INSTALL PLUGIN IF NOT EXISTS` / `UNINSTALL PLUGIN IF EXISTS` — MySQL 9.0+ syntax;
  in MySQL 8.0 these produce ERROR 1064; must check via `information_schema.PLUGINS`

---

## 2026-06-06 — Workflow Fix: Dynamic Matrix for PPA Build

**Action:** Fixed `ppa-build.yml` after merge to `master`, when a release PR revealed
the workflow file was invalid on push to the release branch.

**Key finding documented:**
- In GitHub Actions, `job.if` referencing `matrix.*` is validated before matrix expansion.
  This means an apparently valid condition breaks the workflow at the parser/validator level.
  For selecting `noble`/`resolute` via `workflow_dispatch`, a separate preparatory job is
  required — one that generates a JSON matrix and passes it via `needs.<job>.outputs`.

---

## 2026-06-06 — Workflow Fix: Preserve source `.buildinfo` for `debsign`

**Action:** Fixed `ppa-build.yml` after the first production release `v2.2.0`, where
the release-triggered PPA workflow successfully built `noble` and `resolute` source packages
but failed at the signing step.

**Key finding documented:**
- `debsign` expects `*_source.buildinfo` alongside `*_source.changes`. If a multi-job
  workflow transfers only `.dsc`, `.changes`, `.orig.tar.gz`, and `.debian.tar.*`, signing
  fails before the upload step. `*.buildinfo` must be included as a full source artifact.

---

## 2026-06-06 — Workflow Fix: `dput` SFTP transport requires `python3-paramiko`

**Action:** Fixed the upload job after a manual `workflow_dispatch` run where source
package signing succeeded but the Launchpad upload failed on the GitHub runner.

**Key finding documented:**
- `dput` with `method = sftp` on a GitHub-hosted Ubuntu runner requires `python3-paramiko`
  to be installed. Without it, the upload step fails with
  `paramiko must be installed to use sftp transport`, even if the GPG signature and
  SSH configuration are correct.

---

## 2026-06-06 — Workflow Change: Switch Launchpad upload from SFTP to FTP

**Action:** After SFTP upload reached the real Launchpad connection and hit
`Permission denied (publickey)`, the workflow was switched to the documented FTP path.

**Key finding documented:**
- For GitHub Actions Launchpad uploads, plain FTP (`method = ftp`, `login = anonymous`)
  is simpler and more reliable than SFTP: it requires no separate SSH key lifecycle
  in Launchpad and GitHub secrets, while GPG signature verification of the source
  package is still enforced by `dput`/Launchpad.

---

## 2026-06-06 — Workflow Fix: One `.orig.tar.gz` per upstream across series uploads

**Action:** After switching to FTP it was discovered that the `noble` upload succeeded
but the second upload for `resolute` failed immediately after the first package in the
same job.

**Key finding documented:**
- For multi-series uploads of the same upstream version, the full source (`-sa`) must be
  included only once; subsequent series must use `-sd`. Otherwise Launchpad may reject
  repeated orig uploads or behave inconsistently on the second transfer.
- It is also safer to call `dput` separately for each `*_source.changes` rather than
  passing multiple files in one shell-expanded invocation.

---

## 2026-06-06 — Workflow Fix: Pin manual uploads to an explicit source ref

**Action:** After Launchpad rejection letters for `2.2.0-2~noble1`, the workflow was
changed so that a manual upload cannot package a release version from a later `master` commit.

**Key finding documented:**
- If `workflow_dispatch` declares `upstream_version=2.2.0` but `git archive` takes the
  content of a later commit on `master`, the `orig.tar.gz` and `debian.tar.*` no longer
  match the previously uploaded version. Launchpad correctly rejects such an upload as
  reuse of the same version number with different contents.

---

## 2026-06-06 — Launchpad Recovery: Burned upstream orig version in PPA

**Action:** After the `source_ref` fix, Launchpad still rejected `2.2.0-3~*` because a
previously uploaded `pinba-engine_2.2.0.orig.tar.gz` with different contents already
existed in the PPA.

**Key finding documented:**
- If the PPA already contains an `orig.tar.gz` for upstream version `2.2.0` with different
  contents, the upstream namespace `2.2.0` in that PPA is effectively burned for further
  corrected uploads.
- The correct recovery for this project was straightforward: cut a new upstream release
  and use it as a new source namespace. Release `v2.2.1` was accepted by Launchpad:
  `pinba-engine_2.2.1.orig.tar.gz`, `pinba-engine_2.2.1-1~noble1.debian.tar.xz`,
  `pinba-engine_2.2.1-1~noble1.dsc` were accepted.
- `debian-sys-maint` on Ubuntu 24.04 MySQL 8.0 does NOT have `SYSTEM_VARIABLES_ADMIN`;
  the `plugin-load-add` config is the correct primary plugin installation mechanism.
- Compiler `.d` dependency files are the exact source for the minimal vendor headers set.
- `DROP DATABASE` with ENGINE=PINBA tables requires the plugin to be loaded.

**Commits:** `df68221`, `db66097`, `d2a19fb`, `57ed76a`

---

## 2026-06-06 — Ingest: PPA Upload Session (Stages 3–4)

**Action:** Session for uploading `pinba_engine 2.1.2` to Launchpad PPA. Closed Stages 3 and 4
of the internal PPA automation plan.

**Raw document added:**
- `raw/sessions/ppa-upload-session-2026-06-06.md`

**Source page created:**
- `wiki/sources/ppa-upload-session-2026-06-06.md`

**Concept page created:**
- `wiki/concepts/launchpad-ppa-workflow.md` — full workflow: dput config, SSH auth,
  signing, versioning, build monitoring, installing from PPA

**Concept page updated:**
- `wiki/concepts/debian-ppa-packaging.md` — added sections:
  - Creating orig.tar.gz via `git archive` (dpkg-buildpackage does not create it)
  - Clarification of version format (`2.1.2-1~noble1`, not `~ubuntu24.04~mysql8.0`)
  - dput SFTP configuration
  - `rapidjson-dev` required in Build-Depends (MySQL headers → `sdi_fwd.h` → `rapidjson/fwd.h`)
  - GPGKeyTemporarilyNotFoundError on first `add-apt-repository`
  - Duplicate sources (.list vs .sources)

**Key findings documented:**
- `rapidjson-dev` — hidden dependency: stale cmake cache masks it in local builds,
  but Launchpad builds in a clean chroot → fatal error
- `method = sftp` in dput — practically always needed instead of `method = ftp` (for local uploads)
- Launchpad rejects `UNRELEASED` in the changelog distribution field
- `orig.tar.gz` must be created manually via `git archive` before `dpkg-buildpackage -S`

**Session result:** `pinba-engine 2.1.2-2~noble1` published to `ppa:xolegator/packages`;
`sudo apt install pinba-engine-mysql-8.0` works fully automatically.
Build: https://launchpad.net/~xolegator/+archive/ubuntu/packages/+build/32942019
Commit: `a14f8ac`

---

## 2026-06-06 — Implementation: GitHub Actions PPA Build (Stage 5)

**Action:** Added the GitHub Actions workflow for building, signing, and uploading
the Launchpad PPA source package.

**Files added:**
- `.github/workflows/ppa-build.yml` — release/manual workflow for source package build,
  lintian check, GPG signing, Launchpad FTP upload
- `wiki/concepts/github-actions-ppa.md` — workflow design, required secrets, and Launchpad
  upload mechanics

**Wiki updates:**
- `wiki/index.md` — added the new GitHub Actions PPA concept and removed the stage 5 gap
- `wiki/overview.md` — added a project-status note that PPA publication is automated

**Key findings documented:**
- One source package already builds both `pinba-engine-mysql-8.0` and
  `pinba-engine-mysql-8.4`, so the workflow does not need a MySQL-series matrix.
- `dpkg-buildpackage -S -sa` still needs a matching `orig.tar.gz`, so the workflow
  creates it with `git archive` before packaging.
- Launchpad upload uses GPG for package signing and FTP for `dput`.

---

## 2026-06-06 — Implementation: Multi-Distro PPA Automation + Version Monitor

**Action:** Expanded PPA automation to handle Ubuntu release-specific source uploads and
added MySQL version monitoring.

**Files added:**
- `.github/workflows/mysql-version-monitor.yml` — weekly/manual monitor of Ubuntu MySQL availability
- `.github/mysql-versions.json` — tracked MySQL version map for `noble` and `resolute`
- `.github/scripts/check-mysql-versions.sh` — apt-based detector
- `wiki/concepts/github-actions-mysql-version-monitor.md` — monitor workflow rationale and behavior

**Files updated:**
- `.github/workflows/ppa-build.yml` — distro matrix (`noble`, `resolute`), generated
  `debian/pinba-ppa-build.mk`, distro-specific changelog versions, multi-artifact upload
- `debian/rules` — package exclusion and conditional build/install based on generated config
- `wiki/concepts/github-actions-ppa.md` — updated to reflect multi-distro source package flow
- `wiki/concepts/debian-ppa-packaging.md` — documented generated build config and multi-distro versioning

**Key findings documented:**
- Launchpad cannot see GitHub runner matrix env directly; distro-specific series selection
  must travel inside the source package itself.
- `debian/pinba-ppa-build.mk` is sufficient to select `8.0 only`, `8.4 only`, or both,
  without forking `debian/control`.

---

## 2026-05-23 — LINT: docker-tag-strategy.md

**Action:** Removed stale "Tags to Remove (Legacy)" table from `wiki/concepts/docker-tag-strategy.md`.

**Reason:** The three old tags (`8.0-pinboard2`, `8.0.46-pinboard1`, `8.4-lts-pinboard1`) were already
deleted from Docker Hub before anyone used them. Documenting removed tags serves no purpose and
makes the page look like a work-in-progress changelog rather than authoritative reference.

---

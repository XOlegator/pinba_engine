---
title: "Activity Log"
type: log
sources: []
related:
  - wiki/index.md
confidence: high
updated: 2026-06-04
---

# Activity Log

Chronological record of all ingest, query, and lint operations.

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

**Action:** Сессия разработки и отладки Debian/PPA пакетирования pinba_engine.
Закрыты ЭТАП 1.5 и ЭТАП 2 внутреннего плана PPA-автоматизации.

**Raw document added:**
- `raw/sessions/ppa-packaging-session-2026-06-04.md` — лог сессии с диагностикой

**Source page created:**
- `wiki/sources/ppa-packaging-session-2026-06-04.md`

**Concept pages created:**
- `wiki/concepts/debian-ppa-packaging.md` — полный процесс PPA packaging для MySQL плагина:
  структура debian/, схема версионирования, вендоринг headers, lintian, Launchpad
- `wiki/concepts/mysql-postinst-patterns.md` — паттерны и ловушки postinst/prerm:
  каскад аутентификации (debian.cnf → auth_socket), `plugin-load-add` vs `INSTALL PLUGIN`,
  синтаксические ловушки MySQL 8.0 vs 9.0, DROP DATABASE + незагруженный ENGINE
- `wiki/concepts/mysql-vendor-headers-minimal.md` — стратегия минимизации vendor headers:
  анализ .d файлов компилятора, алгоритм whitelist-extraction, результат 1317→162 файлов

**Key findings documented:**
- `INSTALL PLUGIN IF NOT EXISTS` / `UNINSTALL PLUGIN IF EXISTS` — MySQL 9.0+ синтаксис,
  в MySQL 8.0 даёт ERROR 1064; нужна проверка через `information_schema.PLUGINS`

---

## 2026-06-06 — Workflow Fix: Dynamic Matrix for PPA Build

**Action:** Исправлен `ppa-build.yml` после merge в `master`, когда release PR показал
невалидность workflow-файла на push в release branch.

**Key finding documented:**
- В GitHub Actions `job.if`, ссылающийся на `matrix.*`, валидируется до разворота matrix.
  Из-за этого seemingly valid condition ломает workflow на уровне parser/validator.
  Для выбора `noble`/`resolute` по `workflow_dispatch` нужен отдельный preparatory job,
  который генерирует JSON matrix и передаёт её через `needs.<job>.outputs`.

---

## 2026-06-06 — Workflow Fix: Preserve source `.buildinfo` for `debsign`

**Action:** Исправлен `ppa-build.yml` после первого боевого релиза `v2.2.0`, где
release-triggered PPA workflow успешно собрал `noble` и `resolute`, но упал на
подписи source package.

**Key finding documented:**
- `debsign` ожидает `*_source.buildinfo` рядом с `*_source.changes`. Если multi-job
  workflow переносит только `.dsc`, `.changes`, `.orig.tar.gz` и `.debian.tar.*`,
  подпись падает ещё до SSH upload в Launchpad. `*.buildinfo` нужно переносить как
  полноценный source artifact.

---

## 2026-06-06 — Workflow Fix: `dput` SFTP transport requires `python3-paramiko`

**Action:** Исправлен upload job после ручного `workflow_dispatch` прогона, где
подпись source package уже проходила, но сам Launchpad upload падал на GitHub runner.

**Key finding documented:**
- `dput` с `method = sftp` на GitHub-hosted Ubuntu runner требует установленный
  `python3-paramiko`. Без него upload step падает с `paramiko must be installed to
  use sftp transport`, даже если GPG подпись и SSH-конфигурация уже корректны.

---

## 2026-06-06 — Workflow Change: Switch Launchpad upload from SFTP to FTP

**Action:** После того как SFTP upload дошёл до реального соединения с Launchpad и
упёрся в `Permission denied (publickey)`, workflow переведён на documented FTP path.

**Key finding documented:**
- Для GitHub Actions Launchpad FTP upload (`method = ftp`, `login = anonymous`) проще
  и надёжнее SFTP path: он не требует отдельного SSH key lifecycle в Launchpad и
  GitHub secrets, при этом проверка подписи source package всё равно остаётся на
  стороне `dput`/Launchpad.
- `debian-sys-maint` на Ubuntu 24.04 MySQL 8.0 НЕ имеет `SYSTEM_VARIABLES_ADMIN`;
  `plugin-load-add` конфиг — правильный основной механизм установки плагина
- Compiler `.d` dependency files — точный источник минимального набора vendor headers
- `DROP DATABASE` с ENGINE=PINBA таблицами требует загруженного плагина

**Commits:** `df68221`, `db66097`, `d2a19fb`, `57ed76a`

---

## 2026-06-06 — Ingest: PPA Upload Session (ЭТАП 3–4)

**Action:** Сессия загрузки pinba_engine 2.1.2 в Launchpad PPA. Закрыты ЭТАП 3 и ЭТАП 4 внутреннего плана PPA-автоматизации.

**Raw document added:**
- `raw/sessions/ppa-upload-session-2026-06-06.md`

**Source page created:**
- `wiki/sources/ppa-upload-session-2026-06-06.md`

**Concept page created:**
- `wiki/concepts/launchpad-ppa-workflow.md` — полный рабочий процесс: dput SFTP конфиг,
  SSH аутентификация, подписание, версионирование, мониторинг сборки, установка из PPA

**Concept page updated:**
- `wiki/concepts/debian-ppa-packaging.md` — добавлены секции:
  - Создание orig.tar.gz через `git archive` (dpkg-buildpackage не создаёт сам)
  - Уточнение версионного формата (`2.1.2-1~noble1`, не `~ubuntu24.04~mysql8.0`)
  - dput SFTP конфигурация (FTP порт 21 заблокирован)
  - `rapidjson-dev` обязателен в Build-Depends (MySQL headers → sdi_fwd.h → rapidjson/fwd.h)
  - GPGKeyTemporarilyNotFoundError при первом `add-apt-repository`
  - Дублирование sources (.list vs .sources)

**Key findings documented:**
- `rapidjson-dev` — скрытая зависимость: стale cmake-кэш маскирует её при локальной сборке,
  но Launchpad строит в чистом chroot → fatal error
- `method = sftp` в dput — практически всегда нужен вместо `method = ftp`
- Launchpad не принимает `UNRELEASED` в distribution поле changelog
- orig.tar.gz создавать вручную через `git archive` перед `dpkg-buildpackage -S`

**Итог сессии:** `pinba-engine 2.1.2-2~noble1` опубликован в PPA `ppa:xolegator/packages`,
установка `sudo apt install pinba-engine-mysql-8.0` работает полностью автоматически.
Build: https://launchpad.net/~xolegator/+archive/ubuntu/packages/+build/32942019
Commit: `a14f8ac`

---

## 2026-06-06 — Implementation: GitHub Actions PPA Build (ЭТАП 5)

**Action:** Added the GitHub Actions workflow for building, signing, and uploading
the Launchpad PPA source package.

**Files added:**
- `.github/workflows/ppa-build.yml` — release/manual workflow for source package build,
  lintian check, GPG signing, Launchpad SFTP upload
- `wiki/concepts/github-actions-ppa.md` — workflow design, required secrets, and launchpad
  upload mechanics

**Wiki updates:**
- `wiki/index.md` — added the new GitHub Actions PPA concept and removed the stage 5 gap
- `wiki/overview.md` — added a project-status note that PPA publication is automated

**Key findings documented:**
- One source package already builds both `pinba-engine-mysql-8.0` and
  `pinba-engine-mysql-8.4`, so the workflow does not need a MySQL-series matrix.
- `dpkg-buildpackage -S -sa` still needs a matching `orig.tar.gz`, so the workflow
  creates it with `git archive` before packaging.
- Launchpad upload uses GPG for package signing and SSH/SFTP for `dput`.

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
- MySQL version monitoring should open issues, not auto-upload, because a fresh Launchpad
  upload still needs an explicit Debian revision bump.

---

## 2026-05-23 — LINT: docker-tag-strategy.md

**Action:** Removed stale "Tags to Remove (Legacy)" table from `wiki/concepts/docker-tag-strategy.md`.

**Reason:** The three old tags (`8.0-pinboard2`, `8.0.46-pinboard1`, `8.4-lts-pinboard1`) were already
deleted from Docker Hub before anyone used them. Documenting removed tags serves no purpose and
makes the page look like a work-in-progress changelog rather than authoritative reference.

---

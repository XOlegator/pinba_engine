---
title: "Activity Log"
type: log
sources: []
related:
  - wiki/index.md
confidence: high
updated: 2026-05-23
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

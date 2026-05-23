---
title: "xolegator/pinba_engine Fork — Build System & Docker"
type: source
sources:
  - raw/docs/xolegator-fork-build-docker.md
related:
  - wiki/concepts/cmake-build-system.md
  - wiki/concepts/docker-build-strategy.md
  - wiki/concepts/mysql-plugin-abi.md
confidence: high
updated: 2026-05-23
---

# xolegator Fork — Build System & Docker

## Fork vs Original

The original (tony2001) used autoconf + Makefile, required MySQL 5.1, Judy, libevent, C++ protobuf.
This fork uses CMake 3.24+, C++23, targets MySQL 8.0 and 8.4, replaced all deps.

## CMake: Why Source Headers Are Required

`sql/handler.h` (MySQL server headers) are **not** in `libmysqlclient-dev`.
The client package only has `mysql.h` (for connecting to MySQL).
The plugin build needs server internals — specifically `handlerton` and `handler` class.

Three strategies (in order of convenience):
1. **Distro package** `mysql-source-8.0` — fast, but not always available
2. **CMake auto-download** `PINBA_DOWNLOAD_MYSQL_SOURCE=ON` — ~500MB, always works
3. **Local path** `PINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.x` — fastest if already extracted

On Ubuntu 24.04: use `rapidjson-dev` (MySQL server headers include RapidJSON internally).

## Docker: Two Files, Not One

`Dockerfile.mysql80` and `Dockerfile.mysql84` are separate intentionally — not parameterized.
Reason: different MySQL base images, different CMake series flag, different SHA256 pins.
Keeping them separate makes each pipeline independently verifiable.

## Why libprotobuf.so Is Explicitly Copied

```dockerfile
COPY --from=builder /usr/lib/x86_64-linux-gnu/libprotobuf.so.* /usr/lib/x86_64-linux-gnu/
```

The runtime MySQL image doesn't have libprotobuf. The plugin dynamically links it.
Copying from builder ensures ABI match (same debian:bookworm base, exact same version).

## Plugin Filename Discrepancy

- CMake build output: `ha_pinba.so` (standard MySQL convention)
- Docker install path: `/usr/lib/mysql/plugin/libpinba_engine.so`
- `INSTALL PLUGIN` SONAME must match the installed filename

The Dockerfile renames on copy. For local installs, either name works — just match SONAME.

## docker-entrypoint-initdb.d

Scripts in this directory run once on first container start (MySQL convention).
`01-init-pinba.sh`:
1. `INSTALL PLUGIN pinba SONAME 'libpinba_engine.so'`
2. `CREATE DATABASE IF NOT EXISTS pinba`
3. Runs `default_tables.sql` to create ENGINE=PINBA tables

See: [[cmake-build-system]], [[docker-build-strategy]]

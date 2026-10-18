---
title: "CMake Build System"
type: concept
sources:
  - raw/docs/xolegator-fork-build-docker.md
related:
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/docker-build-strategy.md
confidence: high
updated: 2026-10-18
---

# CMake Build System

## Why CMake (vs Original Autoconf)

The original tony2001 project used autoconf + Makefile (MySQL 5.1 era).
This fork replaced it with CMake 3.24+ and C++23 because:
- Modern toolchain integration (clang-tidy, sanitizers, presets)
- `FetchContent` for automatic test/benchmark dependencies
- Preset system for reproducible builds
- Better cross-platform support

## Quick Start (most common cases)

```bash
# Ubuntu 24.04 with auto-downloaded MySQL headers:
cmake --preset release-download-mysql-source   # MySQL 8.0
cmake --build --preset release

# MySQL 8.4:
cmake --preset release-download-mysql84-source
cmake --build --preset release
```

## Why MySQL Source Headers Are Required (and Large)

The plugin needs `sql/handler.h` — this is a MySQL **server** header, not a client header.
`libmysqlclient-dev` only provides `mysql.h` (for applications that connect to MySQL as a client).
The full MySQL source is ~500MB, hence opt-in via `PINBA_DOWNLOAD_MYSQL_SOURCE=ON`.

On Ubuntu 24.04, `mysql-source-8.0` package may not exist — use the download option.

## Dependency Installation (Ubuntu 24.04)

```bash
sudo apt-get install -y \
  build-essential cmake git pkg-config wget ca-certificates \
  libprotobuf-dev protobuf-compiler \
  rapidjson-dev \          # needed by MySQL server headers internally
  libssl-dev \             # keep unpinned (conflicts with libssl3 from MySQL image)
  default-libmysqlclient-dev
```

## Key CMake Options

| Option | Default | When to use |
|--------|---------|-------------|
| `PINBA_MYSQL_SERIES` | `8.0` | Set to `8.4` for LTS |
| `PINBA_DOWNLOAD_MYSQL_SOURCE` | OFF | Enable for clean builds without local MySQL |
| `PINBA_MYSQL_SOURCE_DIR` | — | Use local extracted MySQL source tarball |
| `PINBA_WITH_TESTS` | ON | Always on for CI |
| `PINBA_DEBUG` | OFF | Enable for verbose logging |
| `PINBA_ENABLE_SANITIZERS` | OFF | Enable for debug/CI builds |

## Output

- Primary artifact: `build/ha_pinba.so`
- Installed to: `{CMAKE_INSTALL_PREFIX}/lib/mysql/plugin/ha_pinba.so`
- Docker builds use `cmake --install` then copy to MySQL plugin dir

## Pre-commit Hooks

Pre-commit runs `clang-format`, `hadolint`, file checks, and `cmakelint`;
`cppcheck` and `clang-tidy` run at the pre-push stage. Install the system C/C++
and Docker linters locally when working on relevant files.

The CMake hook uses a managed Python environment with `cmakelint==1.4.3` and
`--linelength=120`. It does not depend on a developer's PATH or a globally
installed CMake linter. `cmake-lint` from the `cmakelang` package is a different
tool with different options; substituting it caused a usage error rather than
a source check. The hook deliberately invokes `cmakelint` and pins its package.
See the [upstream cmakelint usage](https://github.com/cmake-lint/cmake-lint#usage).

The October 2026 correction also resolves the existing continuation-indent and
mixed-case command warnings without relaxing lint rules or changing build
options, dependency versions, or artifact properties.

See: [[mysql-plugin-abi]], [[docker-build-strategy]]
